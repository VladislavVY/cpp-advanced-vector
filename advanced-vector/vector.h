#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }
    
    RawMemory(const RawMemory&) = delete;
    
    RawMemory& operator=(const RawMemory& rhs) = delete;
    
    RawMemory(RawMemory&& other) noexcept {
        Swap(other);
    }
    
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            Deallocate(buffer_);
            capacity_ = 0;
            Swap(rhs);
        }
        return *this;
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
   Vector() = default;

   explicit Vector(size_t size)
        : data_(size)
        , size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        // Конструируем элементы в new_data, копируя их из data_
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }
    
    Vector(Vector&& other) noexcept {
        Swap(other);
    }
    
    ~Vector() {
        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    using iterator = T*;
    using const_iterator = const T*;    

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // constexpr оператор if будет вычислен во время компиляции
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

     void Resize(size_t new_size) {
         if (new_size > size_) {
             Reserve(new_size);
             std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
         }
         else {
              std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
         }
         size_ = new_size;
     }
    
   Vector& operator=(const Vector& rhs) {
    if (this != &rhs) {
        if (rhs.size_ > data_.Capacity()) {
            Vector rhs_copy(rhs);
            Swap(rhs_copy);
        } else {
            ResizeToSmallerRhs(rhs);
        }
    }
    return *this;
}
    
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }
    
    template <typename Type>
    void PushBack(Type&& value) {
       EmplaceBack(std::forward<Type>(value));
    }
    
    void PopBack() /* noexcept */ {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }
    
   template <typename... Args>
   T& EmplaceBack(Args&&... args) {
    if (size_ == Capacity()) {
        Emplace(end(), std::forward<Args>(args)...);
    } else {
        T* result = new (data_.GetAddress() + size_) T(std::forward<Args>(args)...);
        ++size_;
        return *result;
    }
    return *(end() - 1);
}

    
template<typename ...Args>
iterator Emplace(const_iterator pos, Args && ...args) {
    assert(pos >= begin() && pos <= end());
    size_t position = pos - begin();
    if (size_ == Capacity()) {
        RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
        new (new_data.GetAddress() + position) T(std::forward<Args>(args)...);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(begin(), position, new_data.GetAddress());
            std::uninitialized_move_n(begin() + position, size_ - position, new_data.GetAddress() + position + 1);
        }
        else {
            try {
                std::uninitialized_copy_n(begin(), position, new_data.GetAddress());
                std::uninitialized_copy_n(begin() + position, size_ - position, new_data.GetAddress() + position + 1);
            }
            catch (...) {
                std::destroy_n(new_data.GetAddress() + position, 1);
                throw;
            }
        }
        std::destroy_n(begin(), size_);
        data_.Swap(new_data);
    }
    else {
        if (size_ != 0) {
            new (data_ + size_) T(std::move(*(end() - 1)));
            try {
                std::move_backward(begin() + position, end(), end() + 1);
            }
            catch (...) {
                std::destroy_n(end(), 1);
                throw;
            }
            std::destroy_at(begin() + position);
        }
        new (data_ + position) T(std::forward<Args>(args)...);
    }
    ++size_;
    return data_ + position;
}

iterator Erase(const_iterator pos) {
    assert(pos >= begin() && pos < end());           
    size_t position = pos - begin();
    std::move(begin() + position + 1, end(), begin() + position);
    std::destroy_at(end() - 1);
    size_-=1;
    return (begin() + position);
}    
    
iterator Insert(const_iterator pos, const T& value) {
    return Emplace(pos, value);
}

iterator Insert(const_iterator pos, T&& value) {
    return Emplace(pos, std::move(value));
}
    
private:
    RawMemory<T> data_;
    size_t size_ = 0;
    
    void ResizeToSmallerRhs(const Vector& rhs) {
        std::copy_n(rhs.begin(), std::min(size_, rhs.size_), begin());
        if (size_ > rhs.size_) {  
            std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
        }
        else { 
            std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
        }
        size_ = rhs.size_;
    }
};
