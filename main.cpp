#include <windows.h>
#include <winuser.h>
#include <commctrl.h>
#include <stdio.h>
#include <process.h>
#include <time.h>

#define WM_MOUSEHWHEEL  526

HHOOK s_mouseHook;
double s_timerFrequency = 0.0;

int s_clicked;
int s_running = 0;
LARGE_INTEGER s_timeLastScroll;
int s_maxVelocity = 100;
double s_velocity = 0;
int s_lastScrollDirection;

int s_debug = 0;

typedef struct tagMOUSEHOOKSTRUCTEX
{
    MOUSEHOOKSTRUCT mouseHookStruct;
    DWORD mouseData;
} MOUSEHOOKSTRUCTEX, *PMOUSEHOOKSTRUCTEX, *LPMOUSEHOOKSTRUCTEX;

void DoScroll(int dir)
{
    s_running = 1;

    while (!s_clicked && s_velocity > 0 && s_lastScrollDirection == dir)
    {
        // calculate scroll amount based on current velocity.
        double amount = s_velocity * 32;

        if (dir < 0)
        {
            amount *= -1;
        }

        if (s_debug > 1)
        {
            printf("injecting mouse event scroll of: %f\n", amount);
        }

        mouse_event(MOUSEEVENTF_WHEEL, 0, 0, amount, 0);

        // calculate the amount to decelerate by.
        double decel = max(0.005, (s_velocity * (s_velocity / 2)) / 8);
        s_velocity -= decel;

        if (s_debug > 1)
        {
            printf("decelerating by: %f  new velocity: %f\n", decel, s_velocity);
        }

        // sleep 8ms as 1000/8 = 120 fps. i.e. 2x monitor refresh rate.
        Sleep(8);
    }

    s_running = 0;
    s_velocity = 0;
    s_timeLastScroll.QuadPart = 0;
}

void ScrollDown(void *)
{
    DoScroll(-1);
}


void ScrollUp(void *)
{
    DoScroll(1);
}

static LRESULT CALLBACK MouseCallback(int code, WPARAM wparam, LPARAM lparam)
{
    if (code != HC_ACTION)
    {
        return CallNextHookEx(s_mouseHook, code, wparam, lparam);
    }

    int discardEvent = FALSE;
    MSLLHOOKSTRUCT* info = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
    switch( wparam )
    {
    case WM_MOUSEWHEEL:
    {
        s_clicked = 0;
        short delta = HIWORD(info->mouseData);
        // scrolling via lines, i.e. user generated.
        if (delta != 0 && (delta % WHEEL_DELTA) == 0)
        {
            discardEvent = TRUE;
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);

            if (s_debug)
            {
                printf("scroll event with delta %d\n", delta);
            }

            int direction = delta < 0 ? -1 : 1;

            double diff = 0;
            if (s_timeLastScroll.QuadPart != 0)
            {
                diff = double(now.QuadPart - s_timeLastScroll.QuadPart) / s_timerFrequency;
                diff = s_maxVelocity / diff;

                if (direction != s_lastScrollDirection)
                {
                    s_velocity = 0;
                }
                else
                {
                    double newVelocity = (s_velocity + diff) / 2.0;
                    if (s_debug)
                    {
                        printf("Old velocity: %f, new: %f\n", s_velocity, newVelocity);
                    }
                    s_velocity = newVelocity;
                    if (s_debug)
                    {
                        printf("diff in time: %f scroll amt: %d, curr velocity: %f\n", diff, delta, s_velocity);
                    }
                }
            }

            if (!s_running && s_velocity > 0)
            {
                if (direction > 0)
                {
                    _beginthread(ScrollUp, 0 ,0);
                }
                else
                {
                    _beginthread(ScrollDown, 0 ,0);
                }
            }

            s_lastScrollDirection = direction;
            s_timeLastScroll = now;
        }
        break;
    }
    case WM_MOUSEHWHEEL:
    {
        // todo:
        break;
    }

    case WM_LBUTTONUP:
    {
        s_clicked = 0;
        printf("mouse left up\n");
        break;
    }

    case WM_LBUTTONDOWN:
    {
        s_clicked = 1;
        printf("mouse left down\n");
        break;
    }

    case WM_RBUTTONUP:
    {
        s_clicked = 0;
        printf("mouse right up\n");
        break;
    }

    case WM_RBUTTONDOWN:
    {
        s_clicked = 1;
        printf("mouse right down\n");
        break;
    }

    case WM_MBUTTONUP:
    {
        s_clicked = 0;
        printf("mouse middle up\n");
        break;
    }

    case WM_MBUTTONDOWN:
    {
        s_clicked = 1;
        printf("mouse middle down\n");
        break;
    }
    }

    if (!discardEvent)
    {
        return CallNextHookEx(s_mouseHook, code, wparam, lparam);
    }
    else
    {
        return 1;
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    struct tagMSG msg;

    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);

    s_timeLastScroll.QuadPart = 0;
    LARGE_INTEGER li;

    QueryPerformanceFrequency(&li);
    s_timerFrequency = double(li.QuadPart) / 1000.0;

    s_mouseHook = SetWindowsHookExA(WH_MOUSE_LL, MouseCallback, 0, 0);

    while (GetMessageA(&msg, 0, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return msg.wParam;
}
