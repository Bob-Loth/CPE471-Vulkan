#pragma once
#include <array>
#include <iostream>
#include <cmath>

//TODO remove or simplify for assignment
typedef std::array<float, 16> Matrix;

void printMat(const Matrix& mat) {
    std::cout << std::endl;
    for (size_t i = 0; i < 4; ++i) {
        std::cout << "| ";
        for (size_t j = 0; j < 4; ++j) {
            std::cout << mat[i * 4 + j] << " ";
        }
        std::cout << "|\n";
    }
}

Matrix createIdentityMat() {
    return { 1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             0, 0, 0, 1};
}

Matrix createTranslateMat(const float& x, const float& y, const float& z) {
    return { 1, 0, 0, 0,
             0, 1, 0, 0,
             0, 0, 1, 0,
             x, y, z, 1 };
}

Matrix createScaleMat(const float& x, const float& y, const float& z) {
    return { x, 0, 0, 0,
             0, y, 0, 0,
             0, 0, z, 0,
             0, 0, 0, 1 };
}

Matrix createRotationMatX(const float& rad) {
    return { 1, 0,        0,         0,
             0, cos(rad), sin(rad), 0,
             0, -sin(rad), cos(rad),  0,
             0, 0,        0,         1 };
}

Matrix createRotationMatY(const float& rad) {
    return { cos(rad),  0, -sin(rad), 0,
             0,         1, 0,        0,
             sin(rad), 0, cos(rad), 0,
             0,         0, 0,        1 };
}

Matrix createRotationMatZ(const float& rad) {
    return { cos(rad), sin(rad), 0, 0,
             -sin(rad), cos(rad),  0, 0,
             0,        0,         1, 0,
             0,        0,         0, 1 };
}

Matrix multMat(const Matrix& matL, const Matrix& matR) {
    Matrix result;
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            //vector dot product.
            // The (i,j)th element of result is the dot product
            // of the ith row of L and jth col of R.
            result[i * 4 + j] = 0;
            for (size_t k = 0; k < 4; ++k) {
                result[i * 4 + j] += matL[k * 4 + j] * matR[i * 4 + k];
            }
        }
    }
    return result;
}

void test_multMat() {
    Matrix A;
    Matrix B;
    for (int i = 0; i < 16; ++i) {
        A[i] = i;
        B[i] = i * i;
    }
    Matrix C = multMat(A, B);
    printMat(C);
}
