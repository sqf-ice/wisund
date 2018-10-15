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
 *  \file Message.cpp
 *  \brief Implementation of the Message class
 */
#include "Console.h"
#include "Message.h"
#include <iostream>
#include <iomanip>
#include <initializer_list>

#include <utility> 

Message::Message(std::initializer_list<uint8_t> b) 
    : std::vector<uint8_t>(b)
{}

Message::Message(const uint8_t *msg, size_t size) 
    : std::vector<uint8_t>{}
    {
        reserve(size);
        for (size_t i=0; i < size; ++i) {
            push_back(msg[i]);
        }
    }

Message::Message(std::vector<uint8_t> v) 
    : std::vector<uint8_t>(v)
    {
    }

Message::Message(std::vector<uint8_t> &&v) 
    : std::vector<uint8_t>(v)
    {
    }

Message& Message::operator+=(const Message& msg) {
    // TODO: optimize this?  Could be more efficient
    // if we resize first and then copy?
    for (const auto byte : msg) {
        push_back(byte);
    }
    return *this;
}

bool Message::isRaw() const { return size() && front() == 0; }

bool Message::isCap() const { return size() && front() == 0x31; }

void Message::setSource(void *src) { source = src; }

std::ostream& operator<<(std::ostream &out, const Message &msg) {
    for (auto byte : msg) {
        out << std::setw(2) << std::hex << std::setfill('0') 
            << static_cast<unsigned>(byte);
    }
    return out;
}

#if 0
int main() {
    Message m{0x06, 0x21, 1};
    std::cout << m << "\n";
    std::cout << (m.isRaw() ? "plain" : "not plain") << "\n";
}
#endif
