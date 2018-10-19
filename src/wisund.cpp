// ===========================================================================
// Copyright (c) 2017, Electric Power Research Institute (EPRI)
// All rights reserved.
//
// wisund ("this software") is licensed under BSD 3-Clause license.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
// *  Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// *  Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// *  Neither the name of EPRI nor the names of its contributors may
//    be used to endorse or promote products derived from this software without
//    specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
// IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
// INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
// NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
// OF SUCH DAMAGE.
//
// This EPRI software incorporates work covered by the following copyright and permission
// notices. You may not use these works except in compliance with their respective
// licenses, which are provided below.
//
// These works are provided by the copyright holders and contributors "as is" and any express or
// implied warranties, including, but not limited to, the implied warranties of merchantability
// and fitness for a particular purpose are disclaimed.
//
// This software relies on the following libraries and licenses:
//
// ###########################################################################
// Boost Software License, Version 1.0
// ###########################################################################
//
// * asio v1.10.8 (https://sourceforge.net/projects/asio/files/)
//
// Boost Software License - Version 1.0 - August 17th, 2003
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
// 

/** 
 *  \file wisund.cpp
 *  \brief implemenation of WiSUN "daemon"
 */

#ifndef SIM
#define SIM 0
#endif 

#ifndef CLI
#define CLI 0
#endif 

#include "wisundConfig.h"
#include "SafeQueue.h"
#include "Console.h"
#include "Router.h"
#if SIM
#include "Simulator.h"
#else
#include "SerialDevice.h"
#include "TunDevice.h"
#include "CaptureDevice.h"
#endif
#include <asio.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>

/*
 * The router routes packets according to rules that is given.  Each rule
 * is of the form (source, destination, predicate) where the predicate is 
 * a member function of the Message class that returns a bool.  
 *
 * Messages from the console generally go to the serial device and vice-versa
 * but there are some exceptions for various kinds of other messages.
 *
 * In particular, capture packets from the serial device go to the 
 * capture device and raw packets from the serial device go to the TUN
 * device.  
 *
 * The simulator works the same way as the wisund code except that 
 * instead of using a real serial port and a real TUN device, both are 
 * simulated. Because the TUN device for the real winsund code only
 * has indirect effects, it is omitted entirely from this simulator
 * and only the radio messages are simulated.
 * 
 * For that reason, no TUN and no CaptureDevice are required, since the 
 * Console and Simulator devices are the only two devices.  
 *
 */

#if !SIM
#if CLI
const std::string name{"wisun-cli"};
#else
const std::string name{"wisund"};
#endif
#else
#if CLI
#error "Cannot specify both CLI and SIM simultaneously."
#endif
const std::string name{"wisunsimd"};
#endif

void usage() {
    std::cout << "Usage: " << name << " [-V] [-e] [-v] [-r] [-d msdelay] [-s] serialport capfilename\n"
        "-V  print version and quit\n"
        "-e  echo packets\n"
        "-v  enable verbose mode\n"
        "-r  raw packets\n"
        "-d  delay (in milliseconds)\n"
        "-s  strict packet checking\n"
        "serialport is the device name of the radio port e.g. /dev/serial0\n"
        "capfilename is the name of the capture file or fifo; can also be /dev/null\n";
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage();
        return 1;
    }
    bool verbose = false;
    bool strict = false;
    bool rawpackets = false;
    bool echo = false;
    std::chrono::milliseconds delay{0};
    int opt = 1;
    while (opt < argc && argv[opt][0] == '-') {
        switch (argv[opt][1]) {
            case 'V':
                std::cout << name << " v" << wisund_VERSION << '\n';
                return 0;
                break;
            case 'v':
                verbose = true;
                break;
            case 'r':
                rawpackets = true;
                break;
            case 's':
                strict = true;
                break;
            case 'e':
                echo = true;
                break;
            case 'd':
                // TODO: error handling if next arg is not a number
                delay = std::chrono::milliseconds{std::atoi(argv[++opt])};
                break;
            default:
                std::cout << "Ignoring uknown option \"" << argv[opt] << "\"\n";
        }
        ++opt;
    }
#if SIM
    // these variables are unused by the simulator
    strict = strict;
#endif
    if (opt >= argc) {
        std::cout << "Error: no device given\n";
        return 0;
    }
    const std::string serialname{argv[opt++]};
    std::cout << "Opening port " << serialname << "\n";
    if (opt >= argc) {
        std::cout << "Error: no capture file name given\n";
        return 0;
    }
    const std::string capfilename{argv[opt++]};
    std::cout << "Opening capture file " << capfilename << "\n";

    Router rtr{};
    Console con{rtr.in()};
#if SIM
    Simulator ser{rtr.in()};
    // rule 1: Everything from the Console goes to the serial port
    rtr.addRule(&con, &ser, &Message::isPlain);
    // rule 2: Everything from serial port goes to the console
    rtr.addRule(&ser, &con, &Message::isPlain);
#else
    TunDevice tun{rtr.in()};
    tun.strict(strict);
    SerialDevice ser{rtr.in(), serialname, 115200};
    CaptureDevice cap{};
    // rule 1: Control messages from the console go to the capture device
    rtr.addRule(&con, &cap, &Message::isControl);
    // rule 2: Everything else from the Console goes to the serial port
    rtr.addRule(&con, &ser, &Message::isPlain);
    // rule 3: Everything from the TUN goes to the serial port
    rtr.addRule(&tun, &ser);
    // rule 4: raw packets from the serial port go to the TUN
    rtr.addRule(&ser, &tun, &Message::isRaw);
    // rule 5: If a capture packet comes from the serial port, it goes to the Capture device
    rtr.addRule(&ser, &cap, &Message::isCap);
    // rule 6: All non-raw, non-capture packets from the serial port goes to the Console
    rtr.addRule(&ser, &con, &Message::isPlain);
#endif
    ser.sendDelay(delay);
    ser.verbosity(verbose);
    ser.setraw(rawpackets);
    con.setEcho(echo);
    ser.hold();
#if SIM
    std::thread serThread{&Simulator::run, &ser, &std::cin, &std::cout};
#else
    std::ofstream capfile(capfilename, std::ios::binary);
    tun.hold();
    cap.hold();
    std::thread serThread{&SerialDevice::run, &ser, &std::cin, &std::cout};
    std::thread tunThread{&TunDevice::run, &tun, &std::cin, &std::cout};
    std::thread capThread{&CaptureDevice::run, &cap, &std::cin, &capfile};
#endif
    rtr.hold();
    std::thread rtrThread{&Router::run, &rtr, &std::cin, &std::cout};
#if CLI
    while (!con.getQuitValue()) {
        con.hold();
        con.run(&std::cin, &std::cout);
        if (con.wantReset()) {
            std::cout << "resetting\n";
        }
    }
#else
    asio::io_service ios;
    // IPv4 address, port 5555
    asio::ip::tcp::acceptor acceptor(ios, 
            asio::ip::tcp::endpoint{asio::ip::tcp::v4(), 5555});
    while (!con.getQuitValue()) {
        con.hold();
        asio::ip::tcp::iostream iss;
        asio::ip::tcp::iostream oss;
        acceptor.accept(*iss.rdbuf());
        /* 
         * we set the output stream's basic_socket_streambuf to use the same
         * underlying socket as the input stream.  Sharing a socket among 
         * threads is not a problem, but sharing a single asio::ip::tcp::iostream
         * is.  Doing so leads to data races.
         */
        oss.rdbuf()->assign(asio::ip::tcp::v4(), iss.rdbuf()->native_handle());
        con.run(&iss, &oss);
        if (con.wantReset()) {
            std::cout << "resetting\n";
        }
    }
#endif
    ser.releaseHold();
    serThread.join();  
    rtr.releaseHold();
    rtrThread.join();  
#if !SIM
    tun.releaseHold();
    tunThread.join();  
    cap.releaseHold();
    capThread.join();  
#endif
}

