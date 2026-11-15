- Frame schedule review fixes (performance and motion plan W3.1). A native host latched hidden at a wake resumes when it
  is visible again: on `WM_SETREDRAW` TRUE, `WM_WINDOWPOSCHANGED` with `SWP_SHOWWINDOW`, `WM_PAINT` or any schedule change
  made while visible, and a host hidden only through an ancestor (a minimized top-level, a hidden parent) looks once a
  second while it holds requests, so a caret in a child host blinks again after its top-level is restored. A hidden
  control keeps its request without being ticked or waking anything and is ticked again when shown, so a transition
  hidden part-way (a button press, a slider value, a tree expansion) lands; a control moved to another host while
  animating is ticked once there. An embedded view whose schedule empties outside a wake starts its next animation
  afresh (application woken, idle gap not counted). Requests made before `Attach` arm the dispatcher. One wake ticks a
  control at most once and honours cancels made earlier in it. Schedule calls from another thread than the attachment
  owner change nothing and set `ERROR_INVALID_THREAD_ID`. Deadlines and wakes share one monotonic clock, so a caret
  keeps its phase when the periodic timer stops after a hitch. A host detached after its thread's dispatcher was
  destroyed no longer touches it, a timer message seen without a failure host lets the timer be armed again, and a
  `Tick` running a modal loop gets nested wakes refused.
