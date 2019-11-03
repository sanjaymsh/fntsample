/* Copyright © 2019 Євгеній Мещеряков <eugen@debian.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef UNICODE_BLOCKS_H
#define UNICODE_BLOCKS_H

#include <string>

class unicode_blocks {
public:
    class block {
    public:
        uint32_t start;
        uint32_t end;
        std::string name;

        bool contains(uint32_t codepoint) const
        {
            return start <= codepoint && codepoint <= end;
        }
    };

    virtual ~unicode_blocks();

    virtual size_t size() const = 0;
    virtual ssize_t find(uint32_t codepoint) const = 0;
    virtual block operator[](size_t index) const = 0;
};

#endif
