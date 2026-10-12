// SPDX-License-Identifier: MIT

// The error that seqlib's readers throw for data they can't read.

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace seqlib
{

// Malformed data: the offset in the data where reading stopped, and the reason. what() gives both.
class ReadError : public std::runtime_error
{
public:
    ReadError(std::size_t offset, const std::string& reason);

    // Returns the offset, in bytes from the start of the data, where reading stopped.
    std::size_t Offset() const;

    // Returns the reason, without the offset.
    const std::string& Reason() const;

private:
    std::size_t offset_;
    std::string reason_;
};

} // namespace seqlib
