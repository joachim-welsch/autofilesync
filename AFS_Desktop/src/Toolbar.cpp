#include "Toolbar.h"
#include "id.h"
#include "utilwx.h"
#include "TextResources.h"
#ifdef _UNIX
    // sudo apt install imagemagick
    // convert stop.bmp stop.xpm
    // edit stop.xpm and rename stop to stop_xpm
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wwrite-strings"

    //#include "../icons/sample_xpm.xpm"
    #include "../bitmaps/new.xpm"
    #include "../bitmaps/edit.xpm"
    #include "../bitmaps/save.xpm"
    #include "../bitmaps/delete.xpm"
    #include "../bitmaps/start.xpm"
    #include "../bitmaps/stop.xpm"
    #include "../bitmaps/help.xpm"

    #pragma GCC diagnostic pop
#endif

using utilwx::tul;

enum
{
    Tool_new,
    Tool_edit,
    Tool_save,
    Tool_delete,
    Tool_start,
	Tool_stop,
    Tool_help,
    Tool_Max
};
wxBitmap toolBarBitmaps[Tool_Max];

wxToolBar* Toolbar::toolbar()
{
	return toolbar_;
}

void Toolbar::create(wxFrame* parent)
{
	toolbar_ = parent->CreateToolBar(wxTB_FLAT | wxTB_NODIVIDER | wxTB_HORIZONTAL, tul(id::ToolBar));

#define INIT_TOOL_BMP(bmp) \
		toolBarBitmaps[Tool_##bmp] = wxBITMAP(bmp)

	INIT_TOOL_BMP(new);
	INIT_TOOL_BMP(edit);
	INIT_TOOL_BMP(save);
	INIT_TOOL_BMP(delete);
	INIT_TOOL_BMP(start);
	INIT_TOOL_BMP(stop);
	INIT_TOOL_BMP(help);

	long style = 0;
	style |= wxTB_TOP;
	toolbar_->SetToolBitmapSize(wxSize(24, 24));
	toolbar_->AddTool(tul(id::EditSync), wxEmptyString, wxBITMAP(edit), _(tbtxt::EditSynchronisation));
	toolbar_->AddTool(tul(id::File_save), wxEmptyString, wxBITMAP(save), _(tbtxt::SaveFile));
	toolbar_->AddTool(tul(id::DeleteSync), wxEmptyString, wxBITMAP(delete), _(tbtxt::DeleteSynchronisation));
	toolbar_->AddTool(tul(id::StartSync), wxEmptyString, wxBITMAP(start), _(tbtxt::StartSync));

	toolbar_->AddStretchableSpace();
	toolbar_->AddTool(tul(id::About), wxEmptyString, wxBITMAP(help), _(tbtxt::Help));
	toolbar_->Realize();
}


void Toolbar::replaceTool(int oldToolId, int newToolId, const wxString& label, const wxBitmap& newBitmap, const wxString& shortHelp)
{
    size_t pos = toolbar_->GetToolPos(oldToolId);
    if (pos != wxNOT_FOUND) {
        toolbar_->DeleteTool(oldToolId);
        toolbar_->InsertTool(pos, newToolId, label, newBitmap, wxBitmapBundle(), wxITEM_NORMAL, shortHelp);
        toolbar_->Realize();
    }
}

void Toolbar::setStarted(bool started)
{
    if (started) {
        replaceTool(tul(id::StartSync), tul(id::StopSync), _(tbtxt::StopSync), wxBITMAP(stop), _(tbtxt::StopSync));
    }
    else {
        replaceTool(tul(id::StopSync), tul(id::StartSync), _(tbtxt::StartSync), wxBITMAP(start), _(tbtxt::StartSync));
    }
}
