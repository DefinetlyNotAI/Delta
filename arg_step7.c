#include "arg_internal.h"

static const char *g_step7RegularWallpapers[] = {
    "img\\creep\\everythingg2.png",
    "img\\creep\\everythingg3.png",
    "img\\creep\\everythingg4.png",
    "img\\creep\\everythingg5.png",
    "img\\creep\\everythingg6.png",
    "img\\creep\\clownguy.webp",
    "img\\creep\\artpicture.webp",
    "img\\creep\\dream.jpeg",
    "img\\creep\\colorbars.png",
    "img\\creep\\worldlol.jpg",
    "img\\creep\\ohno.png",
    "img\\creep\\notclean.webp",
    "img\\creep\\mm7.webp",
    "img\\creep\\somethinglikethis.webp",
    "img\\creep\\white.png"
};

DWORD step7RandomWallpaperDelay(void) {
    return 2000 + nextGlitchRandom() % 2001;
}

void step7ShowWallpaperFrame(void) {
    char relativePath[64];

    switch (g_step7.wallpaperMode) {
        case STEP7_WALLPAPER_HAPPY:
            snprintf(relativePath, sizeof(relativePath),
                     "img\\creep\\happy%u.png",
                     g_step7.wallpaperFrame % 9 + 1);
            break;

        case STEP7_WALLPAPER_HT:
            snprintf(relativePath, sizeof(relativePath),
                     "img\\creep\\ht%u.png",
                     g_step7.wallpaperFrame % 4 + 1);
            break;

        case STEP7_WALLPAPER_GREEN:
            snprintf(relativePath, sizeof(relativePath),
                     "img\\creep\\thisisgreen%u.webp",
                     g_step7.wallpaperFrame % 4 + 1);
            break;

        default:
            return;
    }

    setWallpaperFile(relativePath);
    playStaticOverlay();
}

void step7StartWallpaperEvent(ULONGLONG now) {
    size_t choice = nextGlitchRandom() %
                    (sizeof(g_step7RegularWallpapers) /
                     sizeof(g_step7RegularWallpapers[0]) + 3);

    if (choice < sizeof(g_step7RegularWallpapers) /
        sizeof(g_step7RegularWallpapers[0])) {
        setWallpaperFile(g_step7RegularWallpapers[choice]);
        playStaticOverlay();
        g_step7.nextWallpaperAt = now + step7RandomWallpaperDelay();
        return;
    }

    g_step7.wallpaperFrame = 0;

    if (choice == sizeof(g_step7RegularWallpapers) /
        sizeof(g_step7RegularWallpapers[0])) {
        g_step7.wallpaperMode = STEP7_WALLPAPER_HAPPY;
        g_step7.wallpaperFrameCount = 27;
        g_step7.wallpaperFrameInterval = 100;
    } else if (choice == sizeof(g_step7RegularWallpapers) /
               sizeof(g_step7RegularWallpapers[0]) + 1) {
        g_step7.wallpaperMode = STEP7_WALLPAPER_HT;
        g_step7.wallpaperFrameCount = 4;
        g_step7.wallpaperFrameInterval = 500;
    } else {
        g_step7.wallpaperMode = STEP7_WALLPAPER_GREEN;
        g_step7.wallpaperFrameCount = 16;
        g_step7.wallpaperFrameInterval = 200;
    }

    step7ShowWallpaperFrame();
    g_step7.nextWallpaperAt = now + g_step7.wallpaperFrameInterval;
}

void updateStep7Wallpapers(ULONGLONG now) {
    if (now < g_step7.nextWallpaperAt) {
        return;
    }

    if (g_step7.wallpaperMode == STEP7_WALLPAPER_NONE) {
        step7StartWallpaperEvent(now);
        return;
    }

    g_step7.wallpaperFrame++;

    if (g_step7.wallpaperFrame >= g_step7.wallpaperFrameCount) {
        g_step7.wallpaperMode = STEP7_WALLPAPER_NONE;
        g_step7.nextWallpaperAt = now + step7RandomWallpaperDelay();
        return;
    }

    step7ShowWallpaperFrame();
    g_step7.nextWallpaperAt += g_step7.wallpaperFrameInterval;

    if (g_step7.nextWallpaperAt <= now) {
        g_step7.nextWallpaperAt = now + g_step7.wallpaperFrameInterval;
    }
}

BOOL step7ReminderIsOpen(void) {
    static const wchar_t reminder[] = L"Remember to smile :)\r\n";

    for (int index = 0; index < g_textProcessCount; index++) {
        if (wcscmp(g_textProcesses[index].text, reminder) == 0) {
            return TRUE;
        }
    }

    return FALSE;
}

void updateStep7Reminder(ULONGLONG now) {
    if (g_step7.reminderActive) {
        if (!step7ReminderIsOpen()) {
            g_step7.reminderActive = FALSE;
            g_step7.nextReminderAt = now + 7000;
        }
        return;
    }

    if (now >= g_step7.nextReminderAt &&
        startTextProcess(TEXT_HOST_NOTEPAD, L"Remember to smile :)\r\n")) {
        g_step7.reminderActive = TRUE;
    }
}

void startStep7(ULONGLONG now) {
    ZeroMemory(&g_step7, sizeof(g_step7));
    g_step7.nextWallpaperAt = now;
    g_step7.nextReminderAt = now;
}

void startStep7Countdown(ULONGLONG now) {
    g_step7.countdown = 5;
    g_step7.nextCountdownAt = now + 1000;
    setWallpaperFile("img\\countdown\\5.png");
    playStaticOverlayFor(100);
}

BOOL updateStep7Countdown(ULONGLONG now) {
    char relativePath[64];

    if (now < g_step7.nextCountdownAt) {
        return FALSE;
    }

    if (g_step7.countdown == 0) {
        return TRUE;
    }

    g_step7.countdown--;
    snprintf(relativePath, sizeof(relativePath), "img\\countdown\\%d.png",
             g_step7.countdown);
    setWallpaperFile(relativePath);
    playStaticOverlayFor(100);
    g_step7.nextCountdownAt += 1000;

    if (g_step7.nextCountdownAt <= now) {
        g_step7.nextCountdownAt = now + 1000;
    }

    return FALSE;
}
