(deftype L (Listener-type L B))
(deftype B (Broadcaster-type L B))
(defclass (Listener-type L B))
(defclass (Broadcaster-type L B))
(defclass Listener ((Listener-type Listener Broadcaster)))
(defclass Broadcaster ((Broadcaster-type Listener Broadcaster)))
