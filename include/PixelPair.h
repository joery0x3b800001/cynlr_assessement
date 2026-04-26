#pragma once
/**
 * @file    PixelPair.h
 * @brief   Data token that flows between pipeline blocks.
 *
 * The line-scan camera emits two consecutive pixel values simultaneously
 * every cycle T.  PixelPair is the canonical representation of that pair.
 *
 * Using uint8_t matches the __uint8 type specified in the problem statement.
 * The sentinel flag lets a producer signal end-of-stream without a separate
 * side-channel, keeping inter-block communication self-contained.
 */

#include <cstdint>

namespace cynlr
{
    #pragma pack(push, 1)
    struct PixelPair
    {
        uint8_t pixel1{0}; ///< First  consecutive element (column index 2k-1)
        uint8_t pixel2{0}; ///< Second consecutive element (column index 2k)
        bool eos{false};   ///< End-Of-Stream sentinel - tells consumer to drain and stop
    };
    #pragma pack(pop)

} // namespace cynlr
