#include <cstdint>
#include <iostream>


//foo


/**
  Quality field bitmask for analog output status values
*/
enum class AnalogOutputStatusQuality : uint8_t
{
  /// set when the data is "good", meaning that rest of the system can trust the value
  ONLINE = 0x1,
  /// the quality all points get before we have established communication (or populated) the point
  RESTART = 0x2,
  /// set if communication has been lost with the source of the data (after establishing contact)
  COMM_LOST = 0x4,
  /// set if the value is being forced to a "fake" value somewhere in the system
  REMOTE_FORCED = 0x8,
  /// set if the value is being forced to a "fake" value on the original device
  LOCAL_FORCED = 0x10,
  /// set if a hardware input etc. is out of range and we are using a place holder value
  OVERRANGE = 0x20,
  /// set if calibration or reference voltage has been lost meaning readings are questionable
  REFERENCE_ERR = 0x40,
  /// reserved bit
  RESERVED = 0x80
};

//typedef UInt48Type DNPTime;
typedef uint32_t DNPTime;
enum class AnalogQuality : uint8_t
{
  /// set when the data is "good", meaning that rest of the system can trust the value
  ONLINE = 0x1,
  /// the quality all points get before we have established communication (or populated) the point
  RESTART = 0x2,
  /// set if communication has been lost with the source of the data (after establishing contact)
  COMM_LOST = 0x4,
  /// set if the value is being forced to a "fake" value somewhere in the system
  REMOTE_FORCED = 0x8,
  /// set if the value is being forced to a "fake" value on the original device
  LOCAL_FORCED = 0x10,
  /// set if a hardware input etc. is out of range and we are using a place holder value
  OVERRANGE = 0x20,
  /// set if calibration or reference voltage has been lost meaning readings are questionable
  REFERENCE_ERR = 0x40,
  /// reserved bit
  RESERVED = 0x80
};
/**
  Quality field bitmask for binary values
*/
enum class BinaryQuality : uint8_t
{
  /// set when the data is "good", meaning that rest of the system can trust the value
  ONLINE = 0x1,
  /// the quality all points get before we have established communication (or populated) the point
  RESTART = 0x2,
  /// set if communication has been lost with the source of the data (after establishing contact)
  COMM_LOST = 0x4,
  /// set if the value is being forced to a "fake" value somewhere in the system
  REMOTE_FORCED = 0x8,
  /// set if the value is being forced to a "fake" value on the original device
  LOCAL_FORCED = 0x10,
  /// set if the value is oscillating very quickly and some events are being suppressed
  CHATTER_FILTER = 0x20,
  /// reserved bit
  RESERVED = 0x40,
  /// state bit
  STATE = 0x80
};
/**
  Quality field bitmask for counter values
*/
enum class CounterQuality : uint8_t
{
  /// set when the data is "good", meaning that rest of the system can trust the value
  ONLINE = 0x1,
  /// the quality all points get before we have established communication (or populated) the point
  RESTART = 0x2,
  /// set if communication has been lost with the source of the data (after establishing contact)
  COMM_LOST = 0x4,
  /// set if the value is being forced to a "fake" value somewhere in the system
  REMOTE_FORCED = 0x8,
  /// set if the value is being forced to a "fake" value on the original device
  LOCAL_FORCED = 0x10,
  /// Deprecated flag that indicates value has rolled over
  ROLLOVER = 0x20,
  /// indicates an unusual change in value
  DISCONTINUITY = 0x40,
  /// reserved bit
  RESERVED = 0x80
};

/*
*  Quality field bitmask for frozen counter values
*/
enum class FrozenCounterQuality : uint8_t
{
  /// set when the data is "good", meaning that rest of the system can trust the value
  ONLINE = 0x1,
  /// the quality all points get before we have established communication (or populated) the point
  RESTART = 0x2,
  /// set if communication has been lost with the source of the data (after establishing contact)
  COMM_LOST = 0x4,
  /// set if the value is being forced to a "fake" value somewhere in the system
  REMOTE_FORCED = 0x8,
  /// set if the value is being forced to a "fake" value on the original device
  LOCAL_FORCED = 0x10,
  /// Deprecated flag that indicates value has rolled over
  ROLLOVER = 0x20,
  /// indicates an unusual change in value
  DISCONTINUITY = 0x40,
  /// reserved bit
  RESERVED = 0x80
};
/**
  Quality field bitmask for binary output status values
*/
enum class BinaryOutputStatusQuality : uint8_t
{
  /// set when the data is "good", meaning that rest of the system can trust the value
  ONLINE = 0x1,
  /// the quality all points get before we have established communication (or populated) the point
  RESTART = 0x2,
  /// set if communication has been lost with the source of the data (after establishing contact)
  COMM_LOST = 0x4,
  /// set if the value is being forced to a "fake" value somewhere in the system
  REMOTE_FORCED = 0x8,
  /// set if the value is being forced to a "fake" value on the original device
  LOCAL_FORCED = 0x10,
  /// reserved bit 1
  RESERVED1 = 0x20,
  /// reserved bit 2
  RESERVED2 = 0x40,
  /// state bit
  STATE = 0x80
};


class Flags
{
public:
    Flags() : value(0) {}

    Flags(uint8_t value) : value(value) {}

    inline bool IsSet(BinaryQuality flag) const
    {
        return IsSetAny(flag);
    }
    inline bool IsSet(AnalogQuality flag) const
    {
        return IsSetAny(flag);
    }
    inline bool IsSet(CounterQuality flag) const
    {
        return IsSetAny(flag);
    }
    inline bool IsSet(FrozenCounterQuality flag) const
    {
        return IsSetAny(flag);
    }
    inline bool IsSet(BinaryOutputStatusQuality flag) const
    {
        return IsSetAny(flag);
    }
    inline bool IsSet(AnalogOutputStatusQuality flag) const
    {
        return IsSetAny(flag);
    }

    inline void Set(BinaryQuality flag)
    {
        SetAny(flag);
    }
    inline void Set(AnalogQuality flag)
    {
        SetAny(flag);
    }
    inline void Set(CounterQuality flag)
    {
        SetAny(flag);
    }
    inline void Set(FrozenCounterQuality flag)
    {
        SetAny(flag);
    }
    inline void Set(BinaryOutputStatusQuality flag)
    {
        SetAny(flag);
    }
    inline void Set(AnalogOutputStatusQuality flag)
    {
        SetAny(flag);
    }

    uint8_t value;

protected:
    template<class T> bool IsSetAny(T flag) const
    {
        return (value & static_cast<uint8_t>(flag)) != 0;
    }

    template<class T> void SetAny(T flag)
    {
        value |= static_cast<uint8_t>(flag);
    }
};

/**
  Base class shared by all of the DataPoint types.
*/
class Measurement
{
public:
    Flags flags;  //	bitfield that stores type specific quality information
    DNPTime time; //	timestamp associated with the measurement

protected:
    Measurement() {
        std::cout <<" Measurement created\n";

    }

    Measurement(Flags flags) : flags(flags) {}

    Measurement(Flags flags, DNPTime time) : flags(flags), time(time) {}
};

/// Common subclass to analogs and counters
template<class T> class TypedMeasurement : public Measurement
{
public:
    T value;

    typedef T Type;

protected:
    TypedMeasurement() : Measurement(), value(0) {
        std::cout <<" Typed Measurement created\n";

    }
    TypedMeasurement(Flags flags) : Measurement(flags), value(0) {}
    TypedMeasurement(T value, Flags flags) : Measurement(flags), value(value) {}
    TypedMeasurement(T value, Flags flags, DNPTime time) : Measurement(flags, time), value(value) {}
};

// trying to see how we build up a list of points
class Foo : public TypedMeasurement<bool>
{
public:
   Foo(bool f) : f1(f) {
       std::cout <<" Foo f created\n";

   };
   ~Foo() {
     std::cout <<"Foo f destroyed\n";
   }
   bool f1;

};

class Binary : public TypedMeasurement<bool>
{
public:
    Binary();

    Binary(bool value);

    Binary(Flags flags);

    Binary(Flags flags, DNPTime time);

    Binary(bool value, Flags flags);

    Binary(bool value, Flags flags, DNPTime time);
};

/**
 * Abstract way of visiting elements of a collection
 *
 */
template<class T> class IVisitor
{
public:
    virtual void OnValue(const T& value) = 0;
};

/**
 * A visitor implemented as an abstract functor
 *
 */
template<class T, class Fun> class FunctorVisitor : public IVisitor<T>
{

public:
    FunctorVisitor(const Fun& fun_) : fun(fun_) {}

    virtual void OnValue(const T& value) override final
    {
        fun(value);
    }

private:
    Fun fun;
};

/**
 * An interface representing an abstract immutable collection of things of type T.
 *
 * The user can only read these values via callback to receive each element.
 */
template<class T> class ICollection
{
public:
    /**
     * The number of elements in the collection
     */
    virtual size_t Count() const = 0;

    /**
     * Visit all the elements of a collection
     */
    virtual void Foreach(IVisitor<T>& visitor) const = 0;

    /**
        visit all of the elements of a collection
    */
    template<class Fun> void ForeachItem(const Fun& fun) const
    {
        FunctorVisitor<T, Fun> visitor(fun);
        this->Foreach(visitor);
    }

    /**
        Retrieve the only value from the collection.
    */
    bool ReadOnlyValue(T& value) const
    {
        if (this->Count() == 1)
        {
            auto assignValue = [&value](const T& item) { value = item; };
            this->ForeachItem(assignValue);
            return true;
        }
        else
        {
            return false;
        }
    }
};


/**
 * A simple tuple for pairing Values with an index
 */
template<class T> class Indexed
{
public:
    Indexed(const T& value_, uint16_t index_) : value(value_), index(index_) {}

    Indexed() : value(), index(0) {}

    T value;
    uint16_t index;
};

template<class T> Indexed<T> WithIndex(const T& value, uint16_t index)
{
    return Indexed<T>(value, index);
}

// ------------ Binary ---------------

Binary::Binary() : TypedMeasurement(false, flags::RESTART) {}

Binary::Binary(bool value) : TypedMeasurement(value, flags::GetBinaryFlags(flags::ONLINE, value)) {}

Binary::Binary(Flags flags) : TypedMeasurement(flags::GetBinaryValue(flags), flags) {}

Binary::Binary(Flags flags, DNPTime time) : TypedMeasurement(flags::GetBinaryValue(flags), flags, time) {}

Binary::Binary(bool value, Flags flags) : TypedMeasurement(value, flags::GetBinaryFlags(flags, value)) {}

Binary::Binary(bool value, Flags flags, DNPTime time)
    : TypedMeasurement(value, flags::GetBinaryFlags(flags, value), time)
{
}

int main()
{
    bool b = true;
    Foo boo(b);
    Binary bin(true);

  std::cout << " Hello from main 3\n";
}
