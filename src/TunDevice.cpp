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
// ###########################################################################
// GNU General Public License version 2
// ###########################################################################
//
// * mongoose v6.5 (https://github.com/cesanta/mongoose)
//
// Copyright (c) 2004-2013 Sergey Lyubka
// Copyright (c) 2013-2015 Cesanta Software Limited
// All rights reserved
//
// This software is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation. For the terms of this
// license, see <http://www.gnu.org/licenses/>.
//
// You are free to use this software under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this software under a commercial
// license, as set out in <https://www.cesanta.com/license>.
//

/** 
 *  \file TunDevice.cpp
 *  \brief Implementation of the TunDevice class
 */
#include "TunDevice.h"
#include <thread>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <linux/if_tun.h>

TunDevice::TunDevice(SafeQueue<Message> &input, SafeQueue<Message> &output) :
    Device(input, output),
    fd{0},
    m_verbose{false},
    m_ipv6only{true}
{
    struct ifreq ifr;
    int err;

    if ((fd = open("/dev/net/tun", O_RDWR)) == -1) {
        perror("open /dev/net/tun");
        exit(1);
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN;
    // don't let the kernel pick the name
    strncpy(ifr.ifr_name, "tun0", IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) == -1) {
        perror("ioctl TUNSETIFF");
        close(fd);
        exit(1);
    }
    // After the ioctl call above, the fd is "connected" to tun device
    ioctl(fd, TUNSETNOCSUM, 1);
    // use system for these?
}

TunDevice::~TunDevice() {
    close(fd);
}

int TunDevice::runTx(std::istream *in)
{
    in = in;
    while (wantHold()) {
        startReceive(); 
    }
    return 0;
}

int TunDevice::runRx(std::ostream *out)
{
    out = out;
    Message m{};
    while (wantHold()) {
        wait_and_pop(m);
        send(m);
    }
    return 0;
}

int TunDevice::run(std::istream *in, std::ostream *out)
{
    in = in;
    out = out;
    std::thread t1{&TunDevice::runRx, this, out};
    int status = runTx(in);
    t1.join();
    return status;
}
/* 
 * sample packet
 *
 *  -4: 00 00 86 dd 
 * 00h: 60 0d f3 30 
 * 04h: 00 1f         // payload length
 * 06h: 3a 40 
 * 08h: 20 16 0b d8 
 *      00 00 f1 01 
 *      00 00 00 00 
 *      00 00 01 01 
 * 18h: 20 16 0b d8 
 *      00 00 f1 01
 *      00 00 00 00
 *      00 00 01 02 
 * 28h: 80 00 24 68 2e 83 00 11 77 c7 77 58 00 00 00 00 
 * 38h: ad 6d 08 00 00 00 00 00 10 11 12 13 14 15 16
 *
 */
bool TunDevice::isCompleteIpV6Msg(const Message& msg) const {
    if (m_verbose) {
        std::cout << "size = " << std::hex <<  msg.size() << ", ethertype = "
            << (0u + msg[2]) << (0u + msg[3]) << ", ipv6ver = "
            << (msg[4] & 0xf0u) << ", reported size = "
            << (msg[8] * 256u + msg[9] + 44) << "\n";
    }
    // if we're not being picky, accept everything
    if (!m_ipv6only) {
        return true;
    }
    return msg.size() >= 45 
        && (msg[2] == 0x86 && msg[3] == 0xdd) // IPv6 Ethertype
        && ((msg[4] & 0xf0) == 0x60)       // IPv6 version
        && (msg[8] * 256u + msg[9] + 44 == msg.size()) // complete
        ;
}

void TunDevice::startReceive()
{
    fd_set rfds;
    Message msg{};

    for (bool partialpkt{true}; partialpkt; partialpkt = msg.size() > 0) {
        // watch just our fd
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);

        // wait up to one half second
        timeval tv{0, 500000lu};
        auto retval = select(fd+1, &rfds, NULL, NULL, &tv);
        if (retval == -1) {
            // error in select
            std::cout << "TUN: error in select\n";
            return;
        }
        if (FD_ISSET(fd, &rfds)) { 
            uint8_t buf1[1600];
            size_t len = read(fd, buf1, sizeof(buf1));
            msg += Message{buf1, len};
            if (isCompleteIpV6Msg(msg)) {
                push(msg);
            } 
        } else {
            msg.clear();
            //std::cout << "TUN: select timeout\n";
        }
    }
}

size_t TunDevice::send(const Message &msg)
{
    if (msg.size()) {
        write(fd, msg.data(), msg.size());
    }
    return msg.size();
}

bool TunDevice::verbosity(bool verbose) 
{
    std::swap(verbose, m_verbose);
    return verbose;
}

bool TunDevice::strict(bool strict)
{
    std::swap(strict, m_ipv6only);
    return strict;
}
