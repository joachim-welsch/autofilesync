# SplitterWindow

## both-trees-polled

Both calls must always happen: TreeCtrl::mouse_clicked() only resets its latch once it has been
polled from "the 2 instances" it expects (a static counter shared across all TreeCtrl objects,
reset at count==2). "||" short-circuits and skips the second call whenever the first returns
true, so that reset never fired for the left tree - its latch stayed true forever after one
click, once left_ != right_ in click state.
