#include <cassert>
#include <iostream>
#include <cstddef>
#include <cstring>

/// A simplified substitute for RTTI that allows querying the identity of types.
/// Returns an entity of an unspecified type for which operator== and operator!= are defined.
/// Entities for the same type compare equal. Ordering is not defined.
/// If RTTI is enabled, this solution can wrap typeid() directly.
template <typename>
auto getTypeID() noexcept
{
    static const struct{} placeholder{};
    return static_cast<const void*>(&placeholder);
}
using TypeID = decltype(getTypeID<void>());

#if __cplusplus < 201703L
template <typename T> auto* launder(T* const src) { return src; }
#else
using std::launder;
#endif

/// This interface is useful when the concrete type of the any class is not relevant.
class IAny
{
public:
    IAny() = default;
    IAny(const IAny&) = default;
    IAny(IAny&&) = default;
    IAny& operator=(const IAny&) = default;
    IAny& operator=(IAny&&) = default;

    [[nodiscard]] bool has_value() const noexcept { return type() != getTypeID<void>(); }

    /// Returns a pointer to the contained value unless (the instance is empty OR the type is incorrect).
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, void>::value>>
    [[nodiscard]] T* cast() noexcept
    {
        if (type() == getTypeID<T>())
        {
            return launder(static_cast<T*>(ptr()));
        }
        return nullptr;
    }
    template <typename T, typename = std::enable_if_t<!std::is_same<std::decay_t<T>, void>::value>>
    [[nodiscard]] const T* cast() const noexcept
    {
        if (type() == getTypeID<T>())
        {
            return launder(static_cast<const T*>(ptr()));
        }
        return nullptr;
    }

    /// The result equals getTypeID<void>() if empty.
    [[nodiscard]] virtual TypeID type() const noexcept = 0;

protected:
    [[nodiscard]] virtual       void* ptr() noexcept = 0;
    [[nodiscard]] virtual const void* ptr() const noexcept = 0;

    virtual ~IAny() = default;
};

/// Implementation of the non-throwing versions of std::any_cast.
template <typename T>
const T* any_cast(const IAny* const operand) noexcept { return operand->cast<T>(); }
template <typename T>
T* any_cast(IAny* const operand) noexcept { return operand->cast<T>(); }

template <std::size_t Footprint> class UniqueAny;

namespace detail
{
template <typename T>    struct IsUniqueAny               final { static constexpr bool Value = false; };
template <std::size_t F> struct IsUniqueAny<UniqueAny<F>> final { static constexpr bool Value = true; };
}
template <typename T>
constexpr bool IsUniqueAny = detail::IsUniqueAny<std::decay_t<T>>::Value;

/// UniqueAny is designed for types that cannot be copied but can be moved.
/// The contained type shall be move-initializable.
/// The contained object is stored directly inside the instance of UniqueAny without the use of heap.
/// The Footprint must be large enough to accommodate the stored entity; if it is not large enough, a compile-time error will result.
template <std::size_t Footprint>
class UniqueAny final : public IAny
{
    using Storage = std::aligned_storage_t<Footprint>;
    template <std::size_t F> friend class UniqueAny;

public:
    /// Constructs an empty any by default; has_value() = false.
    UniqueAny() = default;

    /// The type is not copy-constructible. The contained type also does not have to be copy-constructible.
    UniqueAny(const UniqueAny& other) = delete;

    /// An alias for the move assignment operator.
    template <std::size_t F>
    UniqueAny(UniqueAny<F>&& other) { *this = std::move(other); }

    /// Moves an object into this instance. The source cannot be any itself.
    template <typename T, typename = std::enable_if_t<!IsUniqueAny<std::decay_t<T>>>>
    UniqueAny(T&& source) { emplace<T>(std::move(source)); }

    ~UniqueAny() override { reset(); }

    /// The type is not copyable. The contained type also does not have to be copyable.
    UniqueAny& operator=(const UniqueAny& other) = delete;

    /// The move constructor will be invoked to perform the move.
    template <std::size_t F>
    UniqueAny& operator=(UniqueAny<F>&& other)
    {
        static_assert(Footprint >= F, "Moving Any<F> into Any<L> when F > L is not safe as it may cause overflow");
        if (this != static_cast<const void*>(&other))
        {
            reset();
            fn_destroy_ = other.fn_destroy_;
            fn_move_ = other.fn_move_;
            ty_ = other.ty_;
            if (fn_move_ != nullptr)
            {
                fn_move_(&storage_, &other.storage_);
            }
            other.reset();
        }
        return *this;
    }

    template <typename Type, typename... Args, typename = std::enable_if_t<!IsUniqueAny<std::decay_t<Type>>>>
    void emplace(Args&&... as)
    {
        using T = std::decay_t<Type>;
        static_assert(sizeof(T) <= Footprint, "Enlarge the footprint");
        static_assert(alignof(T) <= alignof(Storage), "Insufficient alignment requirement");
        reset();
        new (&storage_) T(std::forward<Args>(as)...);
        fn_destroy_ = [](void* const p) { static_cast<T*>(p)->~T(); };
        fn_move_    = [](void* const sto, void* const src) { new (sto) T(std::move(*static_cast<T*>(src))); };
        ty_ = getTypeID<T>();
    }

    void reset()
    {
        if (fn_destroy_ != nullptr)
        {
            fn_destroy_(ptr());
        }
        fn_destroy_ = nullptr;
        fn_move_ = nullptr;
        ty_ = getTypeID<void>();
    }

    [[nodiscard]] TypeID type() const noexcept override { return ty_; }

private:
    [[nodiscard]]       void* ptr() noexcept override { return &storage_; }
    [[nodiscard]] const void* ptr() const noexcept override { return &storage_; }

    void (*fn_destroy_)(void*) = nullptr;
    void (*fn_move_)(void*, void*) = nullptr;
    TypeID ty_ = getTypeID<void>();
    Storage storage_;
};

/// The instance is always initialized with a valid value, but it may turn valueless if the value is moved.
/// The Any type can be either std::any or a custom alternative.
template <typename Iface, typename Any>
class ImplementationCell final
{
public:
    template<typename Impl, typename = std::enable_if_t<std::is_base_of<Iface, Impl>::value>>
    explicit ImplementationCell(Impl&& object) :
        storage_(std::forward<Impl>(object)),
        fn_getter_mut_([](Any& sto) -> Iface* { return any_cast<Impl>(&sto); }),
        fn_getter_const_([](const Any& sto) -> const Iface* { return any_cast<Impl>(&sto); })
    {}

    /// Behavior undefined if the instance is valueless.
    [[nodiscard]]       Iface* operator->()       { return fn_getter_mut_(storage_); }
    [[nodiscard]] const Iface* operator->() const { return fn_getter_const_(storage_); }

    [[nodiscard]] operator bool() const { return storage_.has_value(); }

private:
    Any storage_;
    Iface* (*fn_getter_mut_)(Any&);
    const Iface* (*fn_getter_const_)(const Any&);
};

/// The buffer is movable but not copyable, because copying the contents of a buffer is considered wasteful.
/// The buffer behaves as if it's empty if the underlying implementation is moved away.
class DynamicBuffer final
{
public:
    static constexpr std::size_t ImplementationFootprint = sizeof(void*) * 8;

    class Iface  /// Lizard-specific implementation hidden from the user.
    {
    public:
        [[nodiscard]] virtual std::size_t copy(const std::size_t offset_bytes,
                                               void* const destination,
                                               const std::size_t length_bytes) const = 0;
        [[nodiscard]] virtual std::size_t size() const = 0;
        Iface() = default;
        Iface(const Iface&) = delete;
        Iface(Iface&&) = default;
        Iface& operator=(const Iface&) = delete;
        Iface& operator=(Iface&&) = delete;
        virtual ~Iface() = default;
    };

    /// Accepts a Lizard-specific implementation of Iface and moves it into the internal storage.
    template<typename T, typename = std::enable_if_t<std::is_base_of<Iface, T>::value>>
    explicit DynamicBuffer(T&& source) : impl_(std::move(source)) {}

    /// Copies a fragment of the specified size at the specified offset out of the buffer.
    /// The request is truncated to prevent out-of-range memory access.
    /// Returns the number of bytes copied.
    /// Does nothing and returns zero if the instance has been moved away.
    [[nodiscard]] std::size_t copy(const std::size_t offset_bytes, void* const destination, const std::size_t length_bytes) const
    {
        return impl_ ? impl_->copy(offset_bytes, destination, length_bytes) : 0;
    }
    /// The number of bytes stored in the buffer (possibly scattered, but this is hidden from the user).
    /// Returns zero if the buffer is moved away.
    [[nodiscard]] std::size_t size() const { return impl_ ? impl_->size() : 0; }

private:
    ImplementationCell<Iface, UniqueAny<ImplementationFootprint>> impl_;
};

int main()
{
    UniqueAny<100> str(std::string("Hello world!"));
    UniqueAny<200> str2 = std::move(str);
    str.emplace<int>(123);
    std::cout << str.cast<int>() << std::endl;
    std::cout << str2.cast<std::string>() << std::endl;

    struct MyCustomBuffer final : public DynamicBuffer::Iface
    {
        MyCustomBuffer(const std::size_t sz, void* const ptr) : size_bytes(sz), data(ptr) {}
        MyCustomBuffer(const MyCustomBuffer&) = delete;
        MyCustomBuffer(MyCustomBuffer&& other) : size_bytes(other.size_bytes), data(other.data)
        {
            other.size_bytes = 0;
            other.data = nullptr;
        }
        ~MyCustomBuffer() override { std::free(data); }

        MyCustomBuffer& operator=(const MyCustomBuffer&) = delete;
        MyCustomBuffer& operator=(MyCustomBuffer&&) = delete;

        std::size_t copy(const std::size_t offset_bytes, void* const destination, const std::size_t length_bytes) const override
        {
            const std::size_t off = std::min(offset_bytes, size_bytes);
            const std::size_t sz = std::min(length_bytes, size_bytes - off);
            std::memmove(destination, static_cast<const char*>(data) + off, sz);
            return sz;
        }
        std::size_t size() const override { return size_bytes; }

        std::size_t size_bytes;
        void* data;
    };

    const char hello[] = "Hello!";
    void* const ptr = std::malloc(sizeof(hello));
    std::memcpy(ptr, hello, sizeof(hello));
    DynamicBuffer my_buffer(MyCustomBuffer{sizeof(hello), ptr});

    DynamicBuffer another_buffer = std::move(my_buffer);
    std::cout << my_buffer.size() << std::endl;  // contents moved, returns zero
    std::cout << another_buffer.size() << std::endl;

    char result[4]{0};
    std::cout << another_buffer.copy(2, result, 3) << " " << result << std::endl;

    return 0;
}
