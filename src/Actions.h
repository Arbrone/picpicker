#pragma once

#include <QKeyEvent>

// Keyboard actions shared by the single viewer, the compare view and the grid.
enum class Action {
    None,
    Next, Prev, First, Last,
    NextGroup, PrevGroup, // jump over the rest of a burst
    Select, Reject, ClearMark,
    Rate,        // arg: 0..5
    Label,       // arg: Label enum value
    Keep,        // select this shot, reject the rest of its burst / compare set
    Back,
    Compare,
    Rotate, Zoom, ZoomIn, ZoomOut, ToggleFocus,
    Help,
};

struct KeyAction {
    Action action = Action::None;
    int arg = 0;
};

inline KeyAction keyAction(const QKeyEvent *event)
{
    int key = event->key();
    const bool shift = event->modifiers() & Qt::ShiftModifier;
    const bool keypad = event->modifiers() & Qt::KeypadModifier;

#ifdef Q_OS_LINUX
    // Use the physical position of the number row, so ratings and labels work without Shift on
    // layouts like AZERTY where that row types "& é \" ' ( - è _ ç à". xkb keycodes 10..19 are
    // the keys 1..0, 20 and 21 the two keys to their right ("-" and "=" on QWERTY).
    const quint32 sc = event->nativeScanCode();
    if (!keypad && sc >= 10 && sc <= 19) key = Qt::Key_0 + int((sc - 9) % 10);
    else if (!keypad && sc == 20) key = Qt::Key_Minus;
    else if (!keypad && sc == 21) key = Qt::Key_Plus;
#endif

    if (key >= Qt::Key_0 && key <= Qt::Key_5) return {Action::Rate, key - Qt::Key_0};
    // 6..9 → Red, Yellow, Green, Blue (same keys as Lightroom).
    if (key >= Qt::Key_6 && key <= Qt::Key_9) return {Action::Label, key - Qt::Key_6 + 1};
    switch (key) {
    case Qt::Key_Right: return {shift ? Action::NextGroup : Action::Next};
    case Qt::Key_Left: return {shift ? Action::PrevGroup : Action::Prev};
    case Qt::Key_PageDown: return {Action::NextGroup};
    case Qt::Key_PageUp: return {Action::PrevGroup};
    case Qt::Key_Home: return {Action::First};
    case Qt::Key_End: return {Action::Last};
    case Qt::Key_Up:
    case Qt::Key_S: return {Action::Select};
    case Qt::Key_Down:
    case Qt::Key_X: return {Action::Reject};
    case Qt::Key_Space: return {Action::ClearMark};
    case Qt::Key_K: return {Action::Keep};
    case Qt::Key_Escape: return {Action::Back};
    case Qt::Key_C: return {Action::Compare};
    case Qt::Key_R: return {Action::Rotate};
    case Qt::Key_Z: return {Action::Zoom};
    case Qt::Key_Plus:
    case Qt::Key_Equal: return {Action::ZoomIn};
    case Qt::Key_Minus: return {Action::ZoomOut};
    case Qt::Key_F: return {Action::ToggleFocus};
    case Qt::Key_H:
    case Qt::Key_Question:
    case Qt::Key_F1: return {Action::Help};
    default: return {};
    }
}

// Actions that change marks must not fire repeatedly while a key is held down.
inline bool isEditAction(Action a)
{
    return a == Action::Select || a == Action::Reject || a == Action::ClearMark || a == Action::Rate
           || a == Action::Label || a == Action::Keep;
}
