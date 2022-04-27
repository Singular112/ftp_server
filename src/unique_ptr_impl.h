#ifndef __X_UNIQUE_PTR_X__
#define __X_UNIQUE_PTR_X__

template<typename _Tp>
struct default_delete
{
	default_delete() { }

	template<typename _Up>
	default_delete(const default_delete<_Up>&) { }

	void operator()(_Tp* __ptr) const { delete __ptr; }
};


template <typename _Tp, typename _Tp_Deleter = default_delete<_Tp>>
class unique_ptr
{
public:
	typedef _Tp*				pointer;
	typedef _Tp_Deleter			deleter_type;

	typedef _Tp* unique_ptr::*	__unspecified_pointer_type;

	pointer m_ptr;
	deleter_type m_deleter;

private:
	unique_ptr(unique_ptr&);
	unique_ptr& operator=(unique_ptr&);

public:
	unique_ptr(_Tp* a, _Tp_Deleter b)
		: m_ptr(a)
		, m_deleter(b)
	{ }

	explicit unique_ptr(pointer __p)
		: m_ptr(__p)
		, m_deleter(deleter_type())
	{ }

	
	unique_ptr(unique_ptr&& right)
	{
		m_ptr = right.m_ptr;
		right.m_ptr = nullptr;
	}

	~unique_ptr() { reset(); }

	_Tp_Deleter get_deleter() { return m_deleter; }

	pointer get() const { return m_ptr; }

	pointer release()
	{
		pointer __p = get();
		m_ptr = 0;
		return __p;
	}

	void reset(pointer __p = pointer())
	{
		if (__p != get())
		{
			get_deleter()(get());
			m_ptr = __p;
		}
	}

	unique_ptr& operator=(__unspecified_pointer_type)
	{
		reset();
		return *this;
	}

	bool operator==(const _Tp* right) const
	{
		return m_ptr == right;
	}

	explicit operator bool() const
	{	// test for non-null pointer
		return (this->m_ptr != pointer());
	}

	pointer operator->() const { return m_ptr; }

	_Tp& operator *() const { return *m_ptr; }

	pointer operator ()() const { return m_ptr; }
};

#endif
