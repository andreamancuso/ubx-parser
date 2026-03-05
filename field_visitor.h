#pragma once

#include <napi.h>
#include <type_traits>
#include <string>
#include "comms/field/tag.h"
#include "comms/util/Tuple.h"

struct FieldToJs {
    Napi::Env& env;
    Napi::Object& obj;

    template <typename TField>
    void operator()(TField& field) {
        using Tag = typename std::decay_t<TField>::CommsTag;
        auto jsVal = toJs(field, Tag{});
        const char* fieldName = field.name();
        if (fieldName && fieldName[0] != '\0') {
            obj.Set(Napi::String::New(env, fieldName), jsVal);
        }
    }

private:
    // Int fields — use getScaled() if the field has a scaling ratio
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Int) {
        using F = std::decay_t<TField>;
        if constexpr (F::hasScaling()) {
            return Napi::Number::New(env, field.template getScaled<double>());
        } else {
            return Napi::Number::New(env, static_cast<double>(field.getValue()));
        }
    }

    // Enum fields
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Enum) {
        using Underlying = std::underlying_type_t<typename std::decay_t<TField>::ValueType>;
        return Napi::Number::New(env, static_cast<double>(static_cast<Underlying>(field.getValue())));
    }

    // Bitmask fields
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Bitmask) {
        return Napi::Number::New(env, static_cast<double>(field.getValue()));
    }

    // Float fields
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Float) {
        return Napi::Number::New(env, static_cast<double>(field.getValue()));
    }

    // String fields
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::String) {
        auto& val = field.getValue();
        return Napi::String::New(env, std::string(val.begin(), val.end()));
    }

    // Bundle fields (struct-like composite)
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Bundle) {
        Napi::Object nested = Napi::Object::New(env);
        FieldToJs nestedVisitor{env, nested};
        comms::util::tupleForEach(field.value(), nestedVisitor);
        return nested;
    }

    // Bitfield fields (packed bits composite)
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Bitfield) {
        Napi::Object nested = Napi::Object::New(env);
        FieldToJs nestedVisitor{env, nested};
        comms::util::tupleForEach(field.value(), nestedVisitor);
        return nested;
    }

    // ArrayList fields (repeated elements)
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::ArrayList) {
        auto& vec = field.getValue();
        Napi::Array arr = Napi::Array::New(env, vec.size());
        for (std::size_t i = 0; i < vec.size(); ++i) {
            arr.Set(static_cast<uint32_t>(i), elementToJs(vec[i]));
        }
        return arr;
    }

    // RawArrayList fields (raw byte arrays)
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::RawArrayList) {
        auto& vec = field.getValue();
        auto buf = Napi::Buffer<uint8_t>::Copy(env, reinterpret_cast<const uint8_t*>(vec.data()), vec.size());
        return buf;
    }

    // Optional fields
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Optional) {
        if (field.isMissing()) {
            return env.Null();
        }
        using InnerTag = typename std::decay_t<decltype(field.field())>::CommsTag;
        return toJs(field.field(), InnerTag{});
    }

    // Variant fields — skip for MVP
    template <typename TField>
    Napi::Value toJs(TField& field, comms::field::tag::Variant) {
        static_cast<void>(field);
        return env.Null();
    }

    // ArrayList element conversion — elements that have CommsTag (Bundle, etc.)
    template <typename TElem>
    auto elementToJs(TElem& elem) -> std::enable_if_t<std::is_class_v<TElem>, Napi::Value> {
        using Tag = typename std::decay_t<TElem>::CommsTag;
        return toJs(elem, Tag{});
    }

    // ArrayList element conversion — raw integral types
    template <typename TElem>
    auto elementToJs(TElem& elem) -> std::enable_if_t<!std::is_class_v<TElem>, Napi::Value> {
        return Napi::Number::New(env, static_cast<double>(elem));
    }
};
