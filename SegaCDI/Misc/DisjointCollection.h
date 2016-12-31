/*
	SegaCDI - Sega Dreamcast cdi image validator.

	DisjointCollection.h - Creates a copy of a collection and provides utilities for
		iterating and enumerating through the collection.

	Dec 30th, 2016
		- Initial creation.
*/

#pragma once
#include "../stdafx.h"
#include <iterator>
#include "FlatMemoryIterator.h"

template<class T>
class DisjointCollection : std::iterator<std::input_iterator_tag, T>
{
private:
	// Our collection pointer.
	T *m_pCollection;

	// Number of elements in the collection.
	size_t m_dwCount;

public:
	DisjointCollection(T* pCollection, size_t dwCount)
	{
		// Allocate a new collection we can copy data to.
		this->m_dwCount = dwCount;
		this->m_pCollection = new T[this->m_dwCount];

		// Loop through the collection and perform a deep copy using the copy constructor. It
		// is not the most reliable method but as long as we are careful what datatypes we consume
		// in this class it shouldn't be a problem.
		for (int i = 0; i < this->m_dwCount; i++)
		{
			// Copy the current element.
			this->m_pCollection[i] = T(pCollection[i]);
		}
	}

	DisjointCollection(const DisjointCollection &other)
	{
		// Allocate a new collection we can copy data to.
		this->m_dwCount = other.m_dwCount;
		this->m_pCollection = new T[this->m_dwCount];

		// Loop through the collection and perform a deep copy using the copy constructor. It
		// is not the most reliable method but as long as we are careful what datatypes we consume
		// in this class it shouldn't be a problem.
		for (int i = 0; i < this->m_dwCount; i++)
		{
			// Copy the current element.
			this->m_pCollection[i] = T(other.m_pCollection[i]);
		}
	}

	~DisjointCollection()
	{
		// Free the collection we allocated.
		delete[] this->m_pCollection;
	}

	size_t size()
	{
		// Return the number of elements in the collection.
		return this->m_dwCount;
	}

	T* operator[](int index)
	{
		// Check to make sure the index is valud.
		if (index < 0 || index >= this->m_dwCount)
			return nullptr;

		// Return a constant reference to the element at the specified index.
		return &this->m_pCollection[index];
	}

	T* get(int index)
	{
		// Use the indexer operator to get the object.
		return this->operator[](index);
	}

	FlatMemoryIterator<T>* CreateIterator()
	{
		// Create a new FlatMemoryIterator with the current collection and return it.
		return new FlatMemoryIterator<T>(this->m_pCollection, this->m_dwCount);
	}
};