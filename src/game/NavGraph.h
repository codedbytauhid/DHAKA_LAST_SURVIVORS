// src/game/NavGraph.h
// A tiny "road map" of the university: a handful of waypoints (nodes) joined by edges.
// Zombies use it ONLY when they cannot walk straight to their target (for example the player is in
// another room). Then they find the shortest route along the graph and follow it node by node:
// classroom -> door -> corridor -> stairs or exit door -> courtyard.
#pragma once
#include <vector>
#include <glm/glm.hpp>

class NavGraph {
public:
    int addNode(const glm::vec3& p);
    void connect(int a, int b);                   // two-way link

    int count() const { return (int)m_nodes.size(); }
    const glm::vec3& pos(int i) const { return m_nodes[i]; }

    // Nearest node by distance (height difference counts triple, so we do not pick the wrong floor).
    int nearest(const glm::vec3& p) const;
    // Node indices ordered from nearest to farthest from p (same distance rule).
    std::vector<int> sortedByDistance(const glm::vec3& p) const;

    // Shortest route (Dijkstra). Result includes both end nodes; empty if no route exists.
    std::vector<int> findPath(int from, int to) const;

    // The hand-placed graph for THE FALL OF BARIDHARA (coordinates match Level.cpp).
    static NavGraph createUniversity();

private:
    float cost(int a, int b) const;
    std::vector<glm::vec3> m_nodes;
    std::vector<std::vector<int>> m_adj;
};
