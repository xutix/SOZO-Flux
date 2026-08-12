# Scenes publish per-node desired lighting state

SOZO Flux uses named, reusable lighting scenes as one-time sets of per-node assignments rather than maintaining one authoritative scene with follower nodes. Scene activation updates only its listed nodes, direct control updates one node, overlapping scenes use last-published state, and offline targets retain desired state for replay after reconnect; this keeps web, Dock, and future voice control on one deterministic command model without follow/independent mode branches.
