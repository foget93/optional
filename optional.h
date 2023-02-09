#include <stdexcept>
#include <utility>

/*
Чтобы вовремя обнаружить ошибки работы с сырой памятью,
используйте инструменты:
clang-tidy и UB sanitizer.
*/
// Исключение этого типа должно генерироватся при обращении к пустому optional
class BadOptionalAccess : public std::exception {
public:
    using exception::exception;

    virtual const char* what() const noexcept override {
        return "Bad optional access";
    }
};

template <typename T>
class Optional {
public:
    Optional() = default;

    Optional(const T& value)
        : data_(), is_initialized_(true),
          value_ptr_(new(&data_[0]) T(value)) {}

    Optional(T&& value)
        : data_(), is_initialized_(true),
          value_ptr_(new(&data_[0]) T(std::move(value))) {}

    Optional(const Optional& other)
        : data_(),
          is_initialized_(other.is_initialized_ ? true : false),
          value_ptr_(other.is_initialized_ ? new(&data_[0]) T(other.Value()) : nullptr) {}

    Optional(Optional&& other)
        : data_(),
          is_initialized_(other.is_initialized_ ? true : false),
          value_ptr_(other.is_initialized_ ? new(&data_[0]) T(std::move(other.Value())) : nullptr) {}

    Optional& operator=(const T& value) {
        if (is_initialized_) {
            *value_ptr_ = value;
        } else {
            value_ptr_ = new (&data_[0]) T(value);
            is_initialized_ = true;
        }
        return *this;
    }

    Optional& operator=(T&& rhs) {
        if (is_initialized_) {
            *value_ptr_ = std::move(rhs);
        } else {
            value_ptr_ = new (&data_[0]) T(std::move(rhs));
            is_initialized_ = true;
        }
        return *this;
    }

    Optional& operator=(const Optional& rhs) {
        if (is_initialized_ && rhs.is_initialized_) {
            *value_ptr_ =  *(rhs.value_ptr_);
        }
        else if (!is_initialized_ && rhs.is_initialized_) {
            value_ptr_ = new (&data_[0]) T(*(rhs.value_ptr_));
            is_initialized_ = true;
        }
        else if (is_initialized_ && !rhs.is_initialized_) {
            value_ptr_->~T();
            is_initialized_ = false;
        }

        return *this;
    }

    Optional& operator=(Optional&& rhs) {
        if (is_initialized_ && rhs.is_initialized_) {
            *value_ptr_ = std::move(*(rhs.value_ptr_));
        }
        else if (!is_initialized_ && rhs.is_initialized_) {
            value_ptr_ = new (&data_[0]) T(std::move(*(rhs.value_ptr_)));
            is_initialized_ = true;
        }
        else if (is_initialized_ && !rhs.is_initialized_) {
            value_ptr_->~T();
            is_initialized_ = false;
        }

        return *this;
    }

    ~Optional() {
        Reset();
    }

    bool HasValue() const {
        return is_initialized_;
    }

    // Операторы * и -> не должны делать никаких проверок на пустоту Optional.
    // Эти проверки остаются на совести программиста
    T& operator*() {
        return *value_ptr_;
    }

    const T& operator*() const {
        return *value_ptr_;
    }

    T* operator->() {
        return value_ptr_;
    }

    const T* operator->() const {
        return value_ptr_;
    }

    // Метод Value() генерирует исключение BadOptionalAccess, если Optional пуст
    T& Value() {
        if(!is_initialized_) {
            throw BadOptionalAccess();
        }
        return *value_ptr_;
    }

    const T& Value() const {
        if(!is_initialized_) {
            throw BadOptionalAccess();
        }
        return *value_ptr_;
    }

    void Reset() {
        if (is_initialized_) {
            value_ptr_->~T();
            is_initialized_ = false;
        }
    }

private:
    // alignas нужен для правильного выравнивания блока памяти
    alignas(T) char data_[sizeof(T)];
    bool is_initialized_ = false;
    T* value_ptr_ = nullptr;
};


/*
Размещающий оператор new

В следующем примере в области стека выделяется массив размера, достаточного для хранения объекта типа Cat.
Затем размещающий оператор new конструирует в этом массиве экземпляр класса Cat.
Перед выходом из main сконструированный вручную объект нужно разрушить, явно вызвав его деструктор:
*/
    /*alignas(Cat) char buf[sizeof(Cat)];
    Cat* cat = new (&buf[0]) Cat("Luna"s, 1);
    cat->SayHello();
    cat->~Cat();*///Когда создаёте объекты размещающим оператором new, всегда используйте явный вызов деструктора.

/*
Буфер, нужный для создания объекта, необязательно должен размещаться в области стека.
Его можно выделить и динамически. Чтобы выделить сырой массив данных в куче, вызывают функцию operator new.
У неё есть несколько перегрузок. В нашем случае полезна эта:
void* operator new(std::size_t size);
Данную глобальную функцию может автоматически переопределить программист.
Она выделяет size байт в динамической памяти и возвращает указатель на эту выделенную область.
Функция возвращает указатель void*. Указатели такого типа могут ссылаться на данные произвольного типа.
Эта функция operator new выделяет память в куче по адресу, выровненному по умолчанию.
В документации есть и другие её версии. С ними легко управлять выравниванием данных в динамической памяти.
Для возврата сырой памяти обратно в кучу служит парная функция operator delete.
Версия функции, которую мы будем использовать, выглядит так:
*/

//    void* buf_dynamic = operator new(sizeof(Cat));
//    Cat* cat_d = new(buf_dynamic) Cat("Murka"s, 4);
//    cat_d->SayHello();
//    cat->~Cat();
//    operator delete(buf_dynamic);
