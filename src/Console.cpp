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
 *  \file Console.cpp
 *  \brief Implementation of the Console class
 */
#include "Console.h"
#include "scanner.h"
#include "Reply.h"
#include <thread>
#include <iterator>
#include <iomanip>
#include <sstream>
#include <ostream>
#include <vector>

Console::Console(SafeQueue<Message> &input, SafeQueue<Message> &output) :
    Device(input, output),
    trace_scanning{false},
    trace_parsing{false},
    real_quit{false},
    want_reset{false},
    want_echo{false}
{}

Console::~Console() = default;

int Console::runTx(std::istream *in) {
    Scanner scanner(in);
    yy::Parser parser(scanner, *this);
    parser.set_debug_level(trace_parsing);
    int status = parser.parse();
    releaseHold();
    return status;
}

int Console::runRx(std::ostream *out) {
    Message m{};
    while (wantHold() || more()) {
        wait_and_pop(m);
        auto d = decode(m);
        (*out) << d;
        out->flush();
        if (want_echo) {
            std::cout << d;
        }
    }
    return 0;
}

int Console::run(std::istream *in, std::ostream *out) {
    want_reset = false;
    std::thread t1{&Console::runRx, this, out};
    int status = runTx(in);
    t1.join();
    return status;
}

void Console::compound(uint8_t cmd, std::vector<uint8_t> &data)
{
    Message m{data};
    m.insert(m.begin(), cmd);
    m.insert(m.begin(), 0x6);
    push(m);
}

void Console::compound(uint8_t cmd, uint8_t data)
{
    push(Message{0x6, cmd, data});
}

void Console::simple(uint8_t cmd)
{
    push(Message{0x6, cmd});
}

void Console::selfInput(const std::vector<uint8_t> &data) 
{
    Message m{data};
    m.insert(m.begin(), 0xED);
    inQ.push(m);
}

void Console::reset() 
{
    want_reset = true; 
    real_quit = false; 
}

void Console::quit() 
{
    real_quit = true; 
}

bool Console::setEcho(bool echo) 
{
    std::swap(echo, want_echo);
    return echo;
}

bool Console::getQuitValue() const 
{
    return real_quit;
}

bool Console::wantReset() const 
{
    return want_reset;
}
