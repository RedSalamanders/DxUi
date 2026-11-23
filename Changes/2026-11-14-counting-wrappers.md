- Added counting COM wrappers for test executables (performance and motion plan W0.1): around the Direct2D factory,
  device and device contexts (up to `ID2D1DeviceContext7`) and the DirectWrite factory, installed through a private
  thread-local factory seam that the embedded and native hosts use and that returns the system factories unchanged when
  no test installs it. They count every creation that reaches Direct2D and DirectWrite by resource kind, and every
  Direct2D draw call in the new `frame.drawCalls` work counter, which is populated only where they are installed. On the
  ComplexUi fixture they see exactly the creations the counted helpers count, kind by kind, on every dirty and clean
  frame, change no pixel, and report a deliberately uncounted brush as a mismatch.
