# Test Fixtures

These project files are stable snapshots used by the automated playback tests.
They are intentionally separate from the user-facing projects in `Demos/`, so
demo changes do not silently alter the regression baseline.

Update a fixture only when the expected playback behavior changes, and run the
complete `BlendingsRegressionSuite` target after doing so.
