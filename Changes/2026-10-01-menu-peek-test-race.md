- The Menu suite's modal-loop peek test no longer races its own driver thread.
  - **The failure.** `TestMenuModalLoopPeeksOnlyLivePopupsWhenAPeekClosesASubmenu` failed once on CI's ARM64 Release runner,
    on a pull request that does not touch menus: "the modal loop goes on peeking after a peek closed the submenu". Its
    driver had passed every step, yet the test kept no peek after the close.
  - **The cause.** The test's hook closes the submenu from the loop's peek of the root popup's state probes, and the submenu
    is destroyed before the hook sets the event the driver waits on. The driver then found the submenu gone at once and
    posted its next state probe while the pass that fired the hook still had the root's probe peek to finish. That peek
    could remove the probe: the root answered it and the driver ended the recording before the loop's next pass peeked
    anything.
  - **What the test does now.** After the hook fires, the driver posts only a wake and waits for the first peek kept after the
    close before it posts another state probe. That also makes true what the test assumes: no state probe is pending when
    the hook closes the submenu, so the rest of that pass goes on through the chain the close changed.
  - **Still a test of the loop.** The wait changes only when the driver posts its probes. A chain walk that kept a copy
    taken before the first peek (the shape #42 fixed) would still peek the closed submenu's window on the pass that fired
    the hook, and the test still fails on any kept peek of a window that is gone.
  - Library behavior is unchanged.
