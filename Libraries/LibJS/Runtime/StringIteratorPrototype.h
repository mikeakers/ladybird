/*
 * Copyright (c) 2020, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibJS/Runtime/Object.h>
#include <LibJS/Runtime/PrototypeObject.h>
#include <LibJS/Runtime/StringIterator.h>

namespace JS {

class JS_API StringIteratorPrototype final : public PrototypeObject<StringIteratorPrototype, StringIterator> {
    JS_PROTOTYPE_OBJECT(StringIteratorPrototype, StringIterator, StringIterator);
    GC_DECLARE_ALLOCATOR(StringIteratorPrototype);

public:
    virtual void initialize(Realm&) override;
    virtual ~StringIteratorPrototype() override = default;

    virtual bool is_string_iterator_prototype() const override { return true; }

private:
    explicit StringIteratorPrototype(Realm&);

    JS_DECLARE_NATIVE_FUNCTION(next);
};

template<>
inline bool Object::fast_is<StringIteratorPrototype>() const { return is_string_iterator_prototype(); }

}
