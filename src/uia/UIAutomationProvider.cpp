#include "BaseUtil.h"
#include "UIAutomationProvider.h"

#include <UIAutomation.h>
#include <assert.h>

#include "UIAutomationStartPageProvider.h"
#include "UIAutomationDocumentProvider.h"
#include "WindowInfo.h"

SumatraUIAutomationProvider::SumatraUIAutomationProvider(const WindowInfo* win)
: m_ref_count(0),
m_win(win), m_canvas_hwnd(win->hwndCanvas),
m_startpage(NULL), m_document(NULL)
{
	this->AddRef(); //We are the only copy

	m_startpage = new SumatraUIAutomationStartPageProvider(m_canvas_hwnd, this);
}
SumatraUIAutomationProvider::~SumatraUIAutomationProvider()
{
	if (m_document)
	{
		m_document->FreeDocument(); // tell that the dm is now invalid
		m_document->Release(); // release our hooks
		m_document = NULL;
	}

	m_startpage->Release();
	m_startpage = NULL;
}

void SumatraUIAutomationProvider::OnDocumentLoad()
{
	assert(!m_document);

	m_document = new SumatraUIAutomationDocumentProvider(m_canvas_hwnd, this);
	m_document->LoadDocument(m_win->dm);
	UiaRaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
}
void SumatraUIAutomationProvider::OnDocumentUnload()
{
	if (m_document)
	{
		m_document->FreeDocument(); // tell that the dm is now invalid
		m_document->Release(); // release our hooks
		m_document = NULL;
		UiaRaiseStructureChangedEvent(this, StructureChangeType_ChildrenInvalidated, NULL, 0);
	}
}
void SumatraUIAutomationProvider::OnSelectionChanged()
{
	if (m_document)
		UiaRaiseAutomationEvent(m_document, UIA_Text_TextSelectionChangedEventId);
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::QueryInterface(const IID & iid,void ** ppvObject)
{
	if (ppvObject == NULL)
		return E_POINTER;

	if (iid == IID_IRawElementProviderSimple)
	{
		*ppvObject = static_cast<IRawElementProviderSimple*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}
	else if (iid == IID_IRawElementProviderFragment)
	{
		*ppvObject = static_cast<IRawElementProviderFragment*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}
	else if (iid == IID_IRawElementProviderFragmentRoot)
	{
		*ppvObject = static_cast<IRawElementProviderFragmentRoot*>(this);
		this->AddRef(); //New copy has entered the universe
		return S_OK;
	}

	*ppvObject = NULL;
	return E_NOINTERFACE;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::AddRef(void)
{
	return ++m_ref_count;
}
ULONG STDMETHODCALLTYPE SumatraUIAutomationProvider::Release(void)
{
	if (--m_ref_count)
		return m_ref_count;

	//Suicide
	delete this;
	return 0;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPatternProvider(PATTERNID patternId,IUnknown **pRetVal)
{
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetPropertyValue(PROPERTYID propertyId,VARIANT *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	if (propertyId == UIA_NamePropertyId)
	{
		pRetVal->vt = VT_BSTR;
		pRetVal->bstrVal = SysAllocString(L"Canvas");
		return S_OK;
	}
	else if (propertyId == UIA_IsKeyboardFocusablePropertyId)
	{
		pRetVal->vt = VT_BOOL;
		pRetVal->boolVal = TRUE;
		return S_OK;
	}
	else if (propertyId == UIA_ControlTypePropertyId)
	{
		pRetVal->vt = VT_I4;
		pRetVal->lVal = UIA_CustomControlTypeId;
		return S_OK;
	}
	else if (propertyId == UIA_NativeWindowHandlePropertyId)
	{
		pRetVal->vt = VT_I4;
		pRetVal->lVal = (LONG)m_canvas_hwnd;
		return S_OK;
	}
	

	pRetVal->vt = VT_EMPTY;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_HostRawElementProvider(IRawElementProviderSimple **pRetVal)
{
	return UiaHostProviderFromHwnd(m_canvas_hwnd,pRetVal);
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_ProviderOptions(ProviderOptions *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	*pRetVal = ProviderOptions_ServerSideProvider;
	return S_OK;
}


HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::Navigate(enum NavigateDirection direction, IRawElementProviderFragment **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;
	
	//No siblings, no parent
	if (direction == NavigateDirection_Parent ||
	    direction == NavigateDirection_NextSibling ||
	    direction == NavigateDirection_PreviousSibling)
	{
	    *pRetVal = NULL;
		return S_OK;
	}
	else if (direction == NavigateDirection_FirstChild ||
	         direction == NavigateDirection_LastChild)
	{
		// return document content element, or the start page element
		if (m_document)
			*pRetVal = m_document;
		else
			*pRetVal = m_startpage;

		if (*pRetVal)
			(*pRetVal)->AddRef();
		return S_OK;
	}
	else
	{
		return E_INVALIDARG;
	}
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetRuntimeId(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//Top-level elements should return NULL
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetEmbeddedFragmentRoots(SAFEARRAY **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//No other roots => return NULL
	*pRetVal = NULL;
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::SetFocus(void)
{
	//okay
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_BoundingRectangle(struct UiaRect *pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	//Return Bounding Rect of the Canvas area
	RECT canvas_rect;
	GetWindowRect(m_canvas_hwnd, &canvas_rect);

	pRetVal->left   = canvas_rect.left;
	pRetVal->top    = canvas_rect.top;
	pRetVal->width  = canvas_rect.right - canvas_rect.left;
	pRetVal->height = canvas_rect.bottom - canvas_rect.top;

	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::get_FragmentRoot(IRawElementProviderFragmentRoot **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

    *pRetVal = this;
	(*pRetVal)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::ElementProviderFromPoint(double x,double y,IRawElementProviderFragment **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	// traverse the tree
	*pRetVal = this->GetElementFromPoint(x,y,this);
	return S_OK;
}
HRESULT STDMETHODCALLTYPE SumatraUIAutomationProvider::GetFocus(IRawElementProviderFragment **pRetVal)
{
	if (pRetVal == NULL)
		return E_POINTER;

	// whichever child exists in the tree has the focus
	if (m_document)
		*pRetVal = m_document;
	else
		*pRetVal = m_startpage;
	if (*pRetVal)
		(*pRetVal)->AddRef();
	return S_OK;
}
IRawElementProviderFragment* SumatraUIAutomationProvider::GetElementFromPoint(double x,double y,IRawElementProviderFragment * root)
{
	if (!root)
		return NULL;

	//Check the children
	IRawElementProviderFragment* it;
	root->Navigate(NavigateDirection_FirstChild,&it);

	while (it)
	{
		UiaRect rect;
		it->get_BoundingRectangle(&rect);

		// step into
		if (rect.left <= x && x <= rect.left+rect.width &&
			rect.top <= y && y <= rect.top+rect.height)
		{
			IRawElementProviderFragment* leaf = GetElementFromPoint(x,y,it);
			it->Release(); //release our copy
			return leaf;
		}
		
		// go to next element, release old one
		IRawElementProviderFragment* old_it = it;
		old_it->Navigate(NavigateDirection_NextSibling,&it);
		old_it->Release();
	}

	//No such child
	root->AddRef();
	return root;
}
