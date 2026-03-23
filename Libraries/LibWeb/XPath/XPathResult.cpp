/*
 * Copyright (c) 2025, Johannes Gustafsson <johannesgu@outlook.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/Realm.h>
#include <LibWeb/Bindings/Intrinsics.h>
#include <LibWeb/Bindings/PlatformObject.h>
#include <LibWeb/Bindings/XPathResultPrototype.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Node.h>
#include <LibWeb/WebIDL/DOMException.h>
#include <LibWeb/WebIDL/ExceptionOr.h>

#include "XPathResult.h"

namespace Web::XPath {

GC_DEFINE_ALLOCATOR(XPathResult);

XPathResult::XPathResult(JS::Realm& realm)
    : Web::Bindings::PlatformObject(realm)
{
    m_node_set_iter = m_node_set.end();
}

void XPathResult::initialize(JS::Realm& realm)
{
    WEB_SET_PROTOTYPE_FOR_INTERFACE(XPathResult);
    Base::initialize(realm);
}

void XPathResult::visit_edges(Cell::Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_node_set);
    visitor.visit(m_document);
    if (!m_node_set_iter.is_end())
        visitor.visit(*m_node_set_iter);
}

void XPathResult::finalize()
{
    Base::finalize();
    if (m_document)
        m_document->unregister_xpath_result({}, *this);
}

XPathResult::~XPathResult() = default;

void XPathResult::set_number(WebIDL::Double number_value)
{
    m_result_type = NUMBER_TYPE;
    m_number_value = number_value;
}
void XPathResult::set_string(String string_value)
{
    m_result_type = STRING_TYPE;
    m_string_value = move(string_value);
}

void XPathResult::set_boolean(bool boolean_value)
{
    m_result_type = BOOLEAN_TYPE;
    m_boolean_value = boolean_value;
}

void XPathResult::set_node_set(Vector<GC::Ptr<DOM::Node>> node_set, unsigned short type, GC::Ptr<DOM::Document> document)
{
    if (m_document)
        m_document->unregister_xpath_result({}, *this);
    m_document = nullptr;
    m_invalid_iterator_state = false;

    if (type >= XPathResult::UNORDERED_NODE_ITERATOR_TYPE && type <= XPathResult::FIRST_ORDERED_NODE_TYPE)
        m_result_type = type;
    else if (type == ANY_TYPE)
        m_result_type = UNORDERED_NODE_ITERATOR_TYPE;
    else if (type == NUMBER_TYPE || type == STRING_TYPE || type == BOOLEAN_TYPE)
        m_result_type = type;
    else
        m_result_type = UNORDERED_NODE_ITERATOR_TYPE; // Default for unrecognized resultType values

    m_node_set = move(node_set);
    m_node_set_iter = m_node_set.begin();

    // https://www.w3.org/TR/DOM-Level-3-XPath/xpath.html#XPathResult
    if (m_result_type == UNORDERED_NODE_ITERATOR_TYPE || m_result_type == ORDERED_NODE_ITERATOR_TYPE) {
        m_document = document;
        if (m_document)
            m_document->register_xpath_result({}, *this);
    }
}

// https://www.w3.org/TR/DOM-Level-3-XPath/xpath.html#XPathResult-iterateNext
WebIDL::ExceptionOr<GC::Ptr<DOM::Node>> XPathResult::iterate_next()
{
    if (m_result_type != UNORDERED_NODE_ITERATOR_TYPE && m_result_type != ORDERED_NODE_ITERATOR_TYPE)
        return WebIDL::SimpleException { WebIDL::SimpleExceptionType::TypeError, "XPathResult is not an iterator type"_string };

    if (m_invalid_iterator_state)
        return WebIDL::InvalidStateError::create(realm(), "The document has been mutated since the XPathResult was returned"_utf16);

    if (m_node_set_iter == m_node_set.end())
        return GC::Ptr<DOM::Node> {};

    return *m_node_set_iter++;
}

GC::Ptr<DOM::Node> XPathResult::snapshot_item(int index)
{
    if (index < 0 || static_cast<size_t>(index) >= m_node_set.size())
        return nullptr;

    return m_node_set.at(index);
}

}
