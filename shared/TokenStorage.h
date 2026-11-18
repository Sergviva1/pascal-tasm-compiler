#pragma once
#include "Token.h"
#include <iostream>
#include <stdexcept>

// Собственное динамическое хранилище токенов.
// STL-контейнеры (vector, list, map и т. п.) не используются.
class TokenStorage {
private:
    Token* data;
    std::size_t count;
    std::size_t capacity;

    void grow() {
        std::size_t newCapacity = (capacity == 0) ? 16 : capacity * 2;
        Token* newData = new Token[newCapacity];

        for (std::size_t i = 0; i < count; ++i)
            newData[i] = data[i];

        delete[] data;
        data = newData;
        capacity = newCapacity;
    }

public:
    TokenStorage() : data(nullptr), count(0), capacity(0) {}

    ~TokenStorage() {
        delete[] data;
    }

    TokenStorage(const TokenStorage&) = delete;
    TokenStorage& operator=(const TokenStorage&) = delete;

    void add(const Token& token) {
        if (count == capacity)
            grow();
        data[count++] = token;
    }

    std::size_t size() const { return count; }

    const Token& operator[](std::size_t index) const {
        if (index >= count)
            throw std::out_of_range("TokenStorage: index out of range");
        return data[index];
    }
};
