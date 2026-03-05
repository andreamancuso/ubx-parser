#pragma once

#include <napi.h>
#include <type_traits>
#include "comms/field/tag.h"
#include "comms/util/Tuple.h"

// Visitor that outputs type metadata (schema) instead of values.
// Returns N-API values: strings for leaf types, objects for composites.
struct SchemaFieldVisitor {
    Napi::Env& env;
    Napi::Object& obj;

    template <typename TField>
    void operator()(TField& field) {
        using Tag = typename std::decay_t<TField>::CommsTag;
        auto schema = toSchema(field, Tag{});
        const char* fieldName = field.name();
        if (fieldName && fieldName[0] != '\0') {
            obj.Set(Napi::String::New(env, fieldName), schema);
        }
    }

private:
    // Int → "number"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::Int) {
        return Napi::String::New(env, "number");
    }

    // Enum → "number"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::Enum) {
        return Napi::String::New(env, "number");
    }

    // Bitmask → "number"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::Bitmask) {
        return Napi::String::New(env, "number");
    }

    // Float → "number"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::Float) {
        return Napi::String::New(env, "number");
    }

    // String → "string"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::String) {
        return Napi::String::New(env, "string");
    }

    // RawArrayList → "Buffer"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::RawArrayList) {
        return Napi::String::New(env, "Buffer");
    }

    // Variant → "null"
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::Variant) {
        return Napi::String::New(env, "null");
    }

    // Bundle (struct-like composite) → nested object
    template <typename TField>
    Napi::Value toSchema(TField& field, comms::field::tag::Bundle) {
        Napi::Object nested = Napi::Object::New(env);
        SchemaFieldVisitor nestedVisitor{env, nested};
        comms::util::tupleForEach(field.value(), nestedVisitor);
        return nested;
    }

    // Bitfield (packed bits composite) → nested object
    template <typename TField>
    Napi::Value toSchema(TField& field, comms::field::tag::Bitfield) {
        Napi::Object nested = Napi::Object::New(env);
        SchemaFieldVisitor nestedVisitor{env, nested};
        comms::util::tupleForEach(field.value(), nestedVisitor);
        return nested;
    }

    // ArrayList → { "$array": elementSchema }
    template <typename TField>
    Napi::Value toSchema(TField&, comms::field::tag::ArrayList) {
        using F = std::decay_t<TField>;
        using ElemType = typename F::ValueType::value_type;
        Napi::Object wrapper = Napi::Object::New(env);
        wrapper.Set("$array", elementSchema<ElemType>());
        return wrapper;
    }

    // Optional → { "$optional": innerSchema }
    template <typename TField>
    Napi::Value toSchema(TField& field, comms::field::tag::Optional) {
        using InnerTag = typename std::decay_t<decltype(field.field())>::CommsTag;
        Napi::Object wrapper = Napi::Object::New(env);
        wrapper.Set("$optional", toSchema(field.field(), InnerTag{}));
        return wrapper;
    }

    // Element schema for class types (Bundle elements, etc.)
    template <typename TElem>
    auto elementSchema() -> std::enable_if_t<std::is_class_v<TElem>, Napi::Value> {
        TElem elem{};
        using Tag = typename std::decay_t<TElem>::CommsTag;
        return toSchema(elem, Tag{});
    }

    // Element schema for raw integral types
    template <typename TElem>
    auto elementSchema() -> std::enable_if_t<!std::is_class_v<TElem>, Napi::Value> {
        return Napi::String::New(env, "number");
    }
};
