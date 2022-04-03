#pragma once
#include <array>
#include <iostream>
#include <cmath>


std::array<float, 16> createIdentityMat() {
    return { 1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             0, 0, 0, 1};
}

std::array<float, 16> createTranslateMat(const float& x, const float& y, const float& z) {
    return { 1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             x, y, z, 1 };
}

std::array<float, 16> createScaleMat(const float& x, const float& y, const float& z) {
    return { 1, 0, 0, x,
             0, 1, 0, y,
             0, 0, 1, z,
             0, 0, 0, 1 };
}

std::array<float, 16> createRotationMatX(const float& rad) {
    return { 1, 0,        0,         0,
             0, cos(rad), -sin(rad), 0,
             0, sin(rad), cos(rad),  0,
             0, 0,        0,         1 };
}

std::array<float, 16> createRotationMatY(const float& rad) {
    return { cos(rad),  0, sin(rad), 0,
             0,         1, 0,        0,
             -sin(rad), 0, cos(rad), 0,
             0,         0, 0,        1 };
}

std::array<float, 16> createRotationMatZ(const float& rad) {
    return { cos(rad), -sin(rad), 0, 0,
             sin(rad), cos(rad),  0, 0,
             0,        0,         1, 0,
             0,        0,         0, 1 };
}


std::array<float, 16> multMat(const std::array<float, 16>& matL, const std::array<float, 16>& matR) {
    std::array<float, 16> result;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            //vector dot.
            // The (i,j)th element of result is the dot product
            // of the ith row of L and jth col of R.
            for (int k = 0; k < 4; ++k) {
                result[i * 4 + j] += matL[i * 4 + k] * matR[k * 4 + j];
            }
        }
    }
    return result;
}

void printMat(const std::array<float, 16>& mat) {
    std::cout << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "| ";
        for (int j = 0; j < 4; ++j) {
            std::cout << mat[i * 4 + j] << " ";
        }
        std::cout << "|\n";
    }
}
