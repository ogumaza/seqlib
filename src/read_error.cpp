// SPDX-License-Identifier: MIT

// The error that seqlib's readers throw (see read_error.h).

#include "seqlib/read_error.h"

#include <string>

namespace seqlib
{

ReadError::ReadError(std::size_t offset, const std::string& reason)
    : std::runtime_error(reason + " (at byte " + std::to_string(offset) + ")"), offset_(offset), reason_(reason)
{
}

std::size_t ReadError::Offset() const
{
    return offset_;
}

const std::string& ReadError::Reason() const
{
    return reason_;
}

} // namespace seqlib
