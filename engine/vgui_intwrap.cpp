#include "quakedef.h"
#include "vgui_int.h"

//SDL defines this
#undef main
#include <VGUI_App.h>

#include "cdll_int.h"

#include "interface.h"

#include "EngineSurface.h"
#include "sound.h"
#include "sys_getmodes.h"
#include "VGUI_EngineSurfaceWrap.h"

SDL_Window* pmainwindow = nullptr;

static EngineSurfaceWrap* staticEngineSurface = nullptr;

static vgui::Panel* staticPanel = nullptr;

void VGuiWrap_SetRootPanelSize()
{
	vgui::Panel* panel = VGuiWrap_GetPanel();

	if (panel)
	{
		POINT pnt;
		RECT rect;

		pnt.x = pnt.y = 0;

		SDL_GetWindowPosition(pmainwindow, (int*)&pnt.x, (int*)&pnt.y);

		rect.top = 0;

		if (VideoMode_IsWindowed())
		{
			SDL_GetWindowSize(pmainwindow, (int*)&rect.right, (int*)&rect.bottom);
		}
		else
		{
			VideoMode_GetCurrentVideoMode((int*)&rect.right, (int*)&rect.bottom, NULL);
		}

		rect.bottom += rect.top;

		panel->setBounds(pnt.x, pnt.y, rect.right, rect.bottom);
	}
}

void VGuiWrap_Startup()
{
	if (staticEngineSurface)
		return;

	vgui::App* pApp = vgui::App::getInstance();

	pApp->reset();

	staticPanel = new vgui::Panel(0, 0, 320, 240);

	vgui::Scheme* pScheme = pApp->getScheme();

	staticPanel->setPaintBorderEnabled(false);
	staticPanel->setPaintBackgroundEnabled(false);
	staticPanel->setPaintEnabled(false);
	staticPanel->setCursor(pScheme->getCursor(vgui::Scheme::scu_none));

	CreateInterfaceFn engineFactory = Sys_GetFactoryThis();

	staticEngineSurface = new EngineSurfaceWrap(staticPanel, (IEngineSurface*)engineFactory(ENGINESURFACE_INTERFACE_VERSION, NULL));

	VGuiWrap_SetRootPanelSize();
}

void VGuiWrap_Shutdown()
{
	delete staticPanel;
	staticPanel = NULL;

	if( staticEngineSurface )
		delete staticEngineSurface;

	staticEngineSurface = NULL;
}

int VGuiWrap_CallEngineSurfaceAppHandler( void* event, void* userData )
{
	if( staticEngineSurface )
		staticEngineSurface->AppHandler( event, userData );

	return FALSE;
}

vgui::Panel* VGuiWrap_GetPanel()
{
	RecEngVGuiWrap_GetPanel();

	return staticPanel;
}

void VGuiWrap_ReleaseMouse()
{
	if( vgui::App::getInstance() && staticEngineSurface )
	{
		if( VGuiWrap2_UseVGUI1() )
		{
			staticEngineSurface->setCursor( 
				vgui::App::getInstance()->
					getScheme()->
						getCursor( vgui::Scheme::scu_arrow ) );

			staticEngineSurface->lockCursor();
		}

		ClientDLL_DeactivateMouse();
		SetMouseEnable( false );
	}
}

void VGuiWrap_GetMouse()
{
	if( staticEngineSurface )
	{
		if( VGuiWrap2_UseVGUI1() )
			staticEngineSurface->unlockCursor();

		ClientDLL_ActivateMouse();
		SetMouseEnable( true );
	}
}

void VGuiWrap_SetVisible( int state )
{
	if( staticPanel )
	{
		staticPanel->setVisible( state );
	}
}

void VGuiWrap_Paint(int paintAll)
{
	vgui::Panel* panel = VGuiWrap_GetPanel();

	if (panel)
	{
		VGuiWrap_SetRootPanelSize();

		panel->repaint();

		vgui::App::getInstance()->externalTick();

		if (paintAll)
		{
			panel->paintTraverse();
		}
		else
		{
			int extents[4];

			panel->getAbsExtents(extents[0], extents[1], extents[2], extents[3]);

			VGui_ViewportPaintBackground(extents);
		}
	}
}

class CDummyApp : public vgui::App
{
public:
	void main( int argc, char* argv[] ) override;

protected:
	void platTick() override;
};

void CDummyApp::main( int argc, char* argv[] )
{
	//Nothing
}

void CDummyApp::platTick()
{
	//Nothing
}

//App for VGUI programs, globally accessed through vgui::App::getInstance().
static CDummyApp theApp;
