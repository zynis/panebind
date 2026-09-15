# R1-C4D relation affordance — PANE_BIND_ORIGINAL_UX

Human Root proposal, not AquaSnap official behavior. Documentation only.
LOCK_UI_IMPLEMENTED = NO; R1C4D_RELATION_AFFORDANCE = NOT_IMPLEMENTED.

A faint lock near a shared edge appears on formation/hover. Left click toggles
relation suppression; right click suppresses the hint. Separating and later
approaching permits a new hint, without creating/deleting geometry truth.

Future model: geometry-derived relation plus SuppressedUntilDetach. Expire
suppression when gap exceeds a justified detach threshold or topology truly
separates. Never persist HWND pairs. Identity lifetime, corner crowding,
accessibility, suppression versus hint state and thresholds need future UAT.

Performance proposal: briefly show a tiny WS_EX_NOACTIVATE overlay on relation
formation; TrackMouseEvent tracks only the overlay's own hover. No global cursor
polling, permanent mouse hook or Core UI. No overlay is implemented here.
