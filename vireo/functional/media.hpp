//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/types.h"
#include "vireo/domain/interval.hpp"
#include "vireo/domain/util.h"
#include "vireo/settings/settings.h"

namespace vireo {
namespace functional {

template <int Type>
class SettingsFunction {
  static_assert(Type == SampleType::Audio ||
                Type == SampleType::Video ||
                Type == SampleType::Data  ||
                Type == SampleType::Caption, "Undefined for this sample type");
protected:
  settings::Settings<Type> _settings = settings::Settings<Type>::None;
public:
  SettingsFunction() {}
  SettingsFunction(const settings::Settings<Type>& settings) : _settings(settings) {}
  auto settings() const -> settings::Settings<Type> { return _settings; }
};

template <typename ObjectType, typename ReturnType, typename ArgType, int Type>
struct Media;

template <typename ObjectType, typename ReturnType>
using DirectAudio = Media<ObjectType, ReturnType, uint32_t, SampleType::Audio>;

template <typename ObjectType, typename ReturnType>
using DirectVideo = Media<ObjectType, ReturnType, uint32_t, SampleType::Video>;

template <typename ObjectType, typename ReturnType>
using DirectData = Media<ObjectType, ReturnType, uint32_t, SampleType::Data>;

template <typename ObjectType, typename ReturnType>
using DirectCaption = Media<ObjectType, ReturnType, uint32_t, SampleType::Caption>;

template <typename ReturnType>
using Audio = Media<Function<ReturnType, uint32_t>, ReturnType, uint32_t, SampleType::Audio>;

template <typename ReturnType>
using Video = Media<Function<ReturnType, uint32_t>, ReturnType, uint32_t, SampleType::Video>;

template <typename ReturnType>
using Data = Media<Function<ReturnType, uint32_t>, ReturnType, uint32_t, SampleType::Data>;

template <typename ReturnType>
using Caption = Media<Function<ReturnType, uint32_t>, ReturnType, uint32_t, SampleType::Caption>;

template <typename ObjectType, typename ReturnType, typename ArgType, int Type>
struct Media : public domain::Interval<ObjectType, ReturnType, ArgType>, public SettingsFunction<Type> {
  Media() : domain::Interval<ObjectType, ReturnType, ArgType>() {}
  Media(const ArgType& a, const ArgType& b) : domain::Interval<ObjectType, ReturnType, ArgType>(a, b) {}
  Media(const ArgType& a, const ArgType& b, const settings::Settings<Type>& settings) : domain::Interval<ObjectType, ReturnType, ArgType>(a, b), SettingsFunction<Type>(settings) {}
  Media(const std::function<ReturnType(ArgType)>& f, const ArgType& a, const ArgType& b, const settings::Settings<Type>& settings)
    : domain::Interval<ObjectType, ReturnType, ArgType>(f, a, b), SettingsFunction<Type>(settings) {}

  // Create from vector and settings
  Media(const vector<ReturnType>& v, const settings::Settings<Type>& settings)
    : domain::Interval<ObjectType, ReturnType, ArgType>(v), SettingsFunction<Type>(settings) {
  }
  // Transform from vector and settings
  template <typename OldReturnType, typename LambdaTransform>
  Media(const vector<OldReturnType>& v, const LambdaTransform& transform, const settings::Settings<Type>& settings)
    : domain::Interval<ObjectType, ReturnType, ArgType>(v, transform), SettingsFunction<Type>(settings) {
  }
  // Create from other and optional new settings
  template <typename T, typename std::enable_if<std::is_class<T>::value && !domain::is_vector<T>::value>::type* = nullptr>
  Media(const T& t, const settings::Settings<Type>& new_settings = settings::Settings<Type>::None)
    : domain::Interval<ObjectType, ReturnType, ArgType>(t), SettingsFunction<Type>(new_settings == settings::Settings<Type>::None ? t.settings() : new_settings) {
  }
  // Transform from other and optional new settings
  template <typename T, typename LambdaTransform, typename std::enable_if<std::is_class<T>::value && !domain::is_vector<T>::value>::type* = nullptr>
  Media(const T& t, const LambdaTransform& transform, const settings::Settings<Type>& new_settings = settings::Settings<Type>::None)
    : domain::Interval<ObjectType, ReturnType, ArgType>(t, transform), SettingsFunction<Type>(new_settings == settings::Settings<Type>::None ? t.settings() : new_settings) {
  }
  // Transform from 2 others and optional new settings
  template <typename T1, typename T2, typename LambdaTransform,
            typename std::enable_if<std::is_class<T1>::value && std::is_class<T2>::value && !domain::is_vector<T1>::value && !domain::is_vector<T2>::value>::type* = nullptr>
  Media(const T1& t1, const T2& t2, const LambdaTransform& transform, const settings::Settings<Type>& new_settings = settings::Settings<Type>::None)
    : domain::Interval<ObjectType, ReturnType, ArgType>(t1, t2, transform), SettingsFunction<Type>(new_settings == settings::Settings<Type>::None ? t1.settings() : new_settings) {
  }

  // Transform 1 interval
  template <typename NewReturnType, typename LambdaTransform>  // Interval in argument is not a vector
  auto transform(const LambdaTransform& transform) -> Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type> {
    return Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type>([f = *static_cast<ObjectType*>(this), transform](ArgType arg) -> NewReturnType {
      return transform(f(arg));
    }, this->a(), this->b(), this->settings());
  }
  template <typename NewReturnType, typename LambdaTransform>  // Interval in argument is not a vector
  auto transform(const LambdaTransform& transform,
                 const std::function<settings::Settings<Type>(settings::Settings<Type>)> settings_transform) -> Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type> {
    return Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type>([f = *static_cast<ObjectType*>(this), transform](ArgType arg) -> NewReturnType {
      return transform(f(arg));
    }, this->a(), this->b(), settings_transform(this->settings()));
  }

  template <typename NewReturnType, typename T, typename LambdaTransform>  // Interval in argument is not a vector
  auto transform_with(const T& t, const LambdaTransform& transform) -> Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type> {
    return Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type>([f1 = *static_cast<ObjectType*>(this), f2 = t, transform](ArgType arg) -> NewReturnType {
      return transform(f1(arg), f2(arg));
    }, std::min(this->a(), t.a()), std::min(this->b(), t.b()), this->settings());
  }
  template <typename NewReturnType, typename T, typename LambdaTransform>  // Interval in argument is not a vector
  auto transform_with(const T& t, const LambdaTransform& transform,
                      const std::function<settings::Settings<Type>(settings::Settings<Type>)> settings_transform) -> Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type> {
    return Media<Function<NewReturnType, ArgType>, NewReturnType, ArgType, Type>([f1 = *static_cast<ObjectType*>(this), f2 = t, transform](ArgType arg) -> NewReturnType {
      return transform(f1(arg), f2(arg));
    }, std::min(this->a(), t.a()), std::min(this->b(), t.b()), settings_transform(this->settings()));
  }

  template <typename LambdaFilter>
  auto filter(const LambdaFilter& filter) -> Media<Function<ReturnType, ArgType>, ReturnType, ArgType, Type> {
    vector<ArgType> mapping;
    ArgType x = this->a();
    for (auto value: *this) {
      if (filter(value)) {
        mapping.push_back(x);
      }
      ++x;
    }
    return Media<Function<ReturnType, ArgType>, ReturnType, ArgType, Type>([f = *static_cast<ObjectType*>(this), mapping](ArgType arg) {
      return f(mapping[arg]);
    }, 0, (ArgType)mapping.size(), this->settings());
  }

  template <typename LambdaFilter>
  auto filter_index(const LambdaFilter& filter) -> Media<Function<ReturnType, ArgType>, ReturnType, ArgType, Type> {
    vector<ArgType> mapping;
    for (ArgType i = this->a(); i < this->b(); ++i) {
      if (filter(i)) {
        mapping.push_back(i);
      }
    }
    return Media<Function<ReturnType, ArgType>, ReturnType, ArgType, Type>([f = *static_cast<ObjectType*>(this), mapping](ArgType arg) {
      return f(mapping[arg]);
    }, 0, (ArgType)mapping.size(), this->settings());
  }

  auto vectorize() const -> vector<ReturnType> {
    vector<ReturnType> out;
    for (const auto& value: *this) {
      out.push_back(value);
    }
    return out;
  }
};

}}
