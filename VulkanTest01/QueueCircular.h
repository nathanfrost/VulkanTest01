#pragma once
#include"MemoryUtil.h"
#include"stdArrayUtility.h"

#define QUEUE_CIRCULAR_WASTED_ELEMENTS_NUM (1)//waste at least one slots to allow (m_head==m_tail) to mean "full" rather than track number of items
#define QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL (kElementsMaxNum+QUEUE_CIRCULAR_WASTED_ELEMENTS_NUM)
template<class T, size_t kElementsMaxNum>
class QueueCircular
{
public:
    typedef QueueCircular<T, kElementsMaxNum> ThisDataType;

    //BEG_#QueueCircularIterator
    //typedef T* iterator;
    //typedef const T* const_iterator;
    //typedef T& reference;
    //typedef const T& const_reference;
    //typedef size_t size_type;
    //END_#QueueCircularIterator

    inline QueueCircular()
    {
        assert(kElementsMaxNum >= 1);
        m_head = 0;
        m_tail = QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL - 1;//end of array
    }
    inline void Enqueue(const T& item)
    {
        assert(Size() < kElementsMaxNum);
        m_array[m_head] = item;
        m_head = (m_head + 1) % QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL;
    }
    inline T Dequeue()
    {
        assert(Size() > 0);
        m_tail = (m_tail + 1) % QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL;
        return m_array[m_tail];
    }
    inline void Dequeue(const size_t elementsToDequeueNum)
    {
        assert(Size() >= elementsToDequeueNum);
        assert(elementsToDequeueNum >= 1);
        m_tail = (m_tail + elementsToDequeueNum) % QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL;
    }
    ///@todo: unit test
    int64_t Find(const T& item)///<@todo: note that int64_t is not consistent with size_t used elsewhere; ideally this would be made consistent
    {
        const int64_t size = CastWithAssert<size_t, int64_t>(Size());
        int64_t i = 0;
        for (; i < size; ++i)
        {
            if (operator[](i) == item)
            {
                return i;
            }
        }
        return -1;//not found
    }
    ///@todo: unit test
	inline bool PeekNextItemToDequeue(T*const nextItem) const
	{
        assert(nextItem);
        if (Size() > 0)
        {
            *nextItem = operator[](0);
            return true;
        }
        else
        {
            return false;
        }
	}
    inline bool PeekLastQueuedItem(T*const lastQueuedItem) const
    {
        assert(lastQueuedItem);
        if (Size() > 0)
        {
            *lastQueuedItem = operator[](Size() - 1);
            return true;
        }
        else
        {
            return false;
        }
    }
    inline size_t Size() const
    {
        const size_t size = m_tail >= m_head ? QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL - (m_tail - m_head) : (m_head - m_tail);
        return size - QUEUE_CIRCULAR_WASTED_ELEMENTS_NUM;
    }

    inline bool Empty() const
    {
        return Size() == 0;
    }
    ///@todo: unit test
    inline bool Full() const
    {
        return Size() == kElementsMaxNum;
    }
    inline void Clear()
    {
        m_tail = (m_head == 0) ? QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL - 1 : m_head - 1;
        assert(Empty());
    }
    inline T& operator[] (const size_t i)
    {
        return GetItemReferenceAt(i);
    }
    inline const T &operator[] (const size_t i) const
    {
        return GetItemReferenceAt(i);
    }

    //BEG_#QueueCircularIterator -- ///@todo: this simplistic copy-paste won't work in general, since the pointer will simply be advanced instead of wrapping correctly
    //iterator begin() noexcept
    //{
    //    return &GetItemReferenceAt(0);
    //}
    //const_iterator begin() const noexcept
    //{
    //    return &GetItemReferenceAt(0);
    //}
    //const_iterator cbegin() const noexcept
    //{
    //    return end();
    //}

    //iterator end() noexcept
    //{
    //    return const_cast<iterator>(static_cast<const ThisDataType*>(this)->end());
    //}
    //const_iterator end() const noexcept
    //{
    //    return const_iterator(&GetItemReferenceAt(Size()-1) + 1);
    //}
    //const_iterator cend() const noexcept
    //{
    //    return end();
    //}
    //END_#QueueCircularIterator

private:
    const T& GetItemReferenceAt(const size_t i) const
    {
        assert(Size() > i);
        return m_array[(m_tail + 1 + i) % QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL];
    }
    T& GetItemReferenceAt(const size_t i)
    {
        return const_cast<T&>(static_cast<const ThisDataType*>(this)->GetItemReferenceAt(i));
    }

    size_t m_head, m_tail;
    ArraySafe<T, QUEUE_CIRCULAR_MAX_ELEMENTS_INTERNAL> m_array;
};
