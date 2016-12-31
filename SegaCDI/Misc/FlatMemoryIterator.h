/*
	SegaCDI - Sega Dreamcast cdi image validator.

	FlatMemoryIterator.h - Custom iterator to iterate through a
		collection of objects in a continous block of memory.

	Dec 20th, 2015
		- Initial creation.
*/

#pragma once
#include "../stdafx.h"
#include <iterator>

template<class T>
class FlatMemoryIterator : std::iterator<std::input_iterator_tag, T>
{
	T* m_Begin;
	T* m_Current;
	T* m_End;

	std::size_t m_Count;

public:
	FlatMemoryIterator(T* p_Object, std::size_t p_Count)
		: m_Begin(p_Object),
		m_Current(p_Object)
	{
		// Initialize fields.
		this->m_End = p_Object + p_Count;
		this->m_Count = p_count;
	}

	FlatMemoryIterator(const FlatMemoryIterator& p_Iterator)
		: m_Begin(p_Iterator.m_Begin),
		m_Current(p_Iterator.m_Current),
		m_End(p_Iterator.m_End),
		m_Count(p_Iterator.m_Count)
	{
	}

	// Pre-Increment
	FlatMemoryIterator& operator++()
	{
		++m_Current;
		return this;
	}

	// Post-Increment
	FlatMemoryIterator operator++(int)
	{
		FlatMemoryIterator s_TempObject(*this);
		++m_Current;
		return s_TempObject;
	}

	bool operator==(const FlatMemoryIterator& p_Object)
	{
		return (m_Begin == p_Object.m_Begin &&
			m_Current == p_Object.m_Current &&
			m_End == p_Object.m_End);
	}

	bool operator!=(const FlatMemoryIterator& p_Object)
	{
		return (m_Begin != p_Object.m_Begin ||
			m_Current != p_Object.m_Current ||
			m_End != p_Object.m_End);
	}

	T& operator*()
	{
		return *m_Current;
	}

	std::size_t size()
	{
		return this->m_Count;
	}

	T* begin()
	{
		return this->m_Begin;
	}

	T* current()
	{
		return this->m_Current;
	}

	T* end()
	{
		return this->m_End;
	}
};