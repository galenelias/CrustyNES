#pragma once

template <class T>
class _NoAddRefReleaseOnCComPtr : 
	public T
{
private:
	STDMETHOD_(ULONG, AddRef)()=0;
	STDMETHOD_(ULONG, Release)()=0;
};

template <class T>
class CComPtrBase
{
protected:
	CComPtrBase() throw()
	{
		p = NULL;
	}
	CComPtrBase(_Inout_opt_ T* lp) throw()
	{
		p = lp;
		if (p != NULL)
			p->AddRef();
	}
	void Swap(CComPtrBase& other)
	{
		T* pTemp = p;
		p = other.p;
		other.p = pTemp;
	}
public:
	typedef T _PtrClass;
	~CComPtrBase() throw()
	{
		if (p)
			p->Release();
	}
	operator T*() const throw()
	{
		return p;
	}
	T& operator*() const
	{
		ATLENSURE(p!=NULL);
		return *p;
	}
	//The assert on operator& usually indicates a bug.  If this is really
	//what is needed, however, take the address of the p member explicitly.
	T** operator&() throw()
	{
		return &p;
	}
	_NoAddRefReleaseOnCComPtr<T>* operator->() const throw()
	{
		return (_NoAddRefReleaseOnCComPtr<T>*)p;
	}
	bool operator!() const throw()
	{	
		return (p == NULL);
	}
	bool operator<(_In_opt_ T* pT) const throw()
	{
		return p < pT;
	}
	bool operator!=(_In_opt_ T* pT) const
	{
		return !operator==(pT);
	}
	bool operator==(_In_opt_ T* pT) const throw()
	{
		return p == pT;
	}

	// Release the interface and set to NULL
	void Release() throw()
	{
		T* pTemp = p;
		if (pTemp)
		{
			p = NULL;
			pTemp->Release();
		}
	}
	// Compare two objects for equivalence
	bool IsEqualObject(_Inout_opt_ IUnknown* pOther) throw()
	{
		if (p == NULL && pOther == NULL)
			return true;	// They are both NULL objects

		if (p == NULL || pOther == NULL)
			return false;	// One is NULL the other is not

		CComPtr<IUnknown> punk1;
		CComPtr<IUnknown> punk2;
		p->QueryInterface(__uuidof(IUnknown), (void**)&punk1);
		pOther->QueryInterface(__uuidof(IUnknown), (void**)&punk2);
		return punk1 == punk2;
	}
	// Attach to an existing interface (does not AddRef)
	void Attach(_In_opt_ T* p2) throw()
	{
		if (p)
		{
			ULONG ref = p->Release();
			(ref);
			// Attaching to the same object only works if duplicate references are being coalesced.  Otherwise
			// re-attaching will cause the pointer to be released and may cause a crash on a subsequent dereference.
		}
		p = p2;
	}
	// Detach the interface (does not Release)
	T* Detach() throw()
	{
		T* pt = p;
		p = NULL;
		return pt;
	}
	_Check_return_ HRESULT CopyTo(_COM_Outptr_result_maybenull_ T** ppT) throw()
	{
		if (ppT == NULL)
			return E_POINTER;
		*ppT = p;
		if (p)
			p->AddRef();
		return S_OK;
	}
	_Check_return_ HRESULT SetSite(_Inout_opt_ IUnknown* punkParent) throw()
	{
		return AtlSetChildSite(p, punkParent);
	}
	_Check_return_ HRESULT Advise(
		_Inout_ IUnknown* pUnk, 
		_In_ const IID& iid, 
		_Out_ LPDWORD pdw) throw()
	{
		return AtlAdvise(p, pUnk, iid, pdw);
	}
	_Check_return_ HRESULT CoCreateInstance(
		_In_ REFCLSID rclsid, 
		_Inout_opt_ LPUNKNOWN pUnkOuter = NULL, 
		_In_ DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
	}
#ifdef _ATL_USE_WINAPI_FAMILY_DESKTOP_APP
	_Check_return_ HRESULT CoCreateInstance(
		_In_z_ LPCOLESTR szProgID, 
		_Inout_opt_ LPUNKNOWN pUnkOuter = NULL, 
		_In_ DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		CLSID clsid;
		HRESULT hr = CLSIDFromProgID(szProgID, &clsid);
		if (SUCCEEDED(hr))
			hr = ::CoCreateInstance(clsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&p);
		return hr;
	}
#endif // _ATL_USE_WINAPI_FAMILY_DESKTOP_APP
	template <class Q>
	_Check_return_ HRESULT QueryInterface(_Outptr_ Q** pp) const throw()
	{
		return p->QueryInterface(__uuidof(Q), (void**)pp);
	}
	T* p;
};

template <class T>
class CComPtr : 
	public CComPtrBase<T>
{
public:
	CComPtr() throw()
	{
	}
	CComPtr(_Inout_opt_ T* lp) throw() :
		CComPtrBase<T>(lp)
	{
	}
	CComPtr(_Inout_ const CComPtr<T>& lp) throw() :
		CComPtrBase<T>(lp.p)
	{	
	}
	T* operator=(_Inout_opt_ T* lp) throw()
	{
		if(*this!=lp)
		{
			CComPtr(lp).Swap(*this);
		}
		return *this;
	}
	template <typename Q>
	T* operator=(_Inout_ const CComPtr<Q>& lp) throw()
	{
		if( !IsEqualObject(lp) )
		{
			return static_cast<T*>(AtlComQIPtrAssign((IUnknown**)&p, lp, __uuidof(T)));
		}
		return *this;
	}
	T* operator=(_Inout_ const CComPtr<T>& lp) throw()
	{
		if(*this!=lp)
		{
			CComPtr(lp).Swap(*this);
		}
		return *this;
	}	
	CComPtr(_Inout_ CComPtr<T>&& lp) throw() :	
		CComPtrBase<T>()
	{	
		lp.Swap(*this);
	}	
	T* operator=(_Inout_ CComPtr<T>&& lp) throw()
	{			
		if (*this != lp)
		{
			CComPtr(static_cast<CComPtr&&>(lp)).Swap(*this);
		}
		return *this;		
	}
};


void VerifyHr(HRESULT hr)
{
	if (FAILED(hr))
		throw std::runtime_error("Unexpected bad HRESULT");
}

