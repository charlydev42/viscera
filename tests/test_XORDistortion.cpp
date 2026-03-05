// test_XORDistortion.cpp — Tests for bb::XORDistortion
#include <catch2/catch_test_macros.hpp>
#include "dsp/XORDistortion.h"

using namespace bb;

TEST_CASE("XOR - Bypass when mask is 0", "[xor]")
{
    XORDistortion xor_dist;
    xor_dist.setMask(0);

    REQUIRE(xor_dist.process(0.5f) == 0.5f);
    REQUIRE(xor_dist.process(-0.3f) == -0.3f);
    REQUIRE(xor_dist.process(0.0f) == 0.0f);
}

TEST_CASE("XOR - Max mask produces distorted output", "[xor]")
{
    XORDistortion xor_dist;
    xor_dist.setMask(0xFFFF);

    float out = xor_dist.process(0.5f);
    REQUIRE(std::isfinite(out));
    REQUIRE(out != 0.5f); // Should be distorted
    REQUIRE(out >= -1.0f);
    REQUIRE(out <= 1.0f);
}

TEST_CASE("XOR - NaN guard returns 0", "[xor]")
{
    XORDistortion xor_dist;
    xor_dist.setMask(0x1234);

    float nanVal = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(xor_dist.process(nanVal) == 0.0f);

    float infVal = std::numeric_limits<float>::infinity();
    REQUIRE(xor_dist.process(infVal) == 0.0f);
}

TEST_CASE("XOR - Output clamped to [-1, 1]", "[xor]")
{
    XORDistortion xor_dist;

    for (uint16_t mask = 0; mask <= 0xFF; mask += 0x11)
    {
        xor_dist.setMask(mask);
        for (float input = -1.0f; input <= 1.0f; input += 0.1f)
        {
            float out = xor_dist.process(input);
            REQUIRE(std::isfinite(out));
            REQUIRE(out >= -1.01f);
            REQUIRE(out <= 1.01f);
        }
    }
}
