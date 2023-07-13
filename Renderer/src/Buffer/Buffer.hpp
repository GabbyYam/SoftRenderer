#pragma once

#include <algorithm>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <stdint.h>
namespace soft {
    template <typename T>
    class Buffer {
    public:
        Buffer() : _width(0), _height(0), _buffer(nullptr) {}

        Buffer(size_t w, size_t h) : _width(w), _height(h)
        {
            _buffer = new T[w * h];
        }

        virtual ~Buffer()
        {
            delete[] _buffer;
        }

        virtual void Resize(size_t w, size_t h)
        {
            delete[] _buffer;
            _width  = w;
            _height = h;
            _buffer = new T[w * h];
        }

        virtual T Get(size_t x, size_t y)
        {
            size_t index = x + y * _width;
            if (index < 0 || index >= _width * _height) {
                spdlog::debug("out of buffer range");
                throw new std::runtime_error("out of range");
            }
            return _buffer[x + y * _width];
        }

        virtual void MemSet(T val)
        {
            // for (auto i = 0; i < _width * _height; ++i)
            //     _buffer[i] = val;

            std::fill(_buffer, _buffer + (_width * _height), val);
        }

        virtual void Set(size_t x, size_t y, T val)
        {
            size_t index = x + y * _width;
            Set(index, val);
        }

        virtual void Set(size_t index, T val)
        {
            if (index >= _width * _height) {
                spdlog::debug("out of buffer range");
                throw new std::runtime_error("out of range");
            }
            _buffer[index] = val;
        }

    public:
        size_t _width = 0, _height = 0;
        T*     _buffer = nullptr;
    };
}  // namespace soft