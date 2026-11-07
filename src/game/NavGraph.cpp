// src/game/NavGraph.cpp
#include "game/NavGraph.h"
#include <algorithm>
#include <cmath>

int NavGraph::addNode(const glm::vec3& p)
{
    m_nodes.push_back(p);
    m_adj.emplace_back();
    return (int)m_nodes.size() - 1;
}

void NavGraph::connect(int a, int b)
{
    m_adj[a].push_back(b);
    m_adj[b].push_back(a);
}

float NavGraph::cost(int a, int b) const
{
    float dx = m_nodes[a].x - m_nodes[b].x, dy = m_nodes[a].y - m_nodes[b].y, dz = m_nodes[a].z - m_nodes[b].z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

std::vector<int> NavGraph::sortedByDistance(const glm::vec3& p) const
{
    std::vector<int> idx(m_nodes.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = (int)i;
    auto d2 = [&](int i) {
        float dx = m_nodes[i].x - p.x, dy = (m_nodes[i].y - p.y) * 3.0f, dz = m_nodes[i].z - p.z;
        return dx * dx + dy * dy + dz * dz;
    };
    std::sort(idx.begin(), idx.end(), [&](int a, int b) { return d2(a) < d2(b); });
    return idx;
}

int NavGraph::nearest(const glm::vec3& p) const
{
    std::vector<int> s = sortedByDistance(p);
    return s.empty() ? -1 : s[0];
}

std::vector<int> NavGraph::findPath(int from, int to) const
{
    std::vector<int> path;
    int n = (int)m_nodes.size();
    if (from < 0 || to < 0 || from >= n || to >= n) return path;

    std::vector<float> dist(n, 1.0e9f);
    std::vector<int> prev(n, -1);
    std::vector<bool> done(n, false);
    dist[from] = 0.0f;
    for (int iter = 0; iter < n; ++iter) {
        int u = -1;
        for (int i = 0; i < n; ++i)
            if (!done[i] && (u < 0 || dist[i] < dist[u])) u = i;
        if (u < 0 || dist[u] >= 1.0e9f) break;
        done[u] = true;
        if (u == to) break;
        for (int v : m_adj[u]) {
            float nd = dist[u] + cost(u, v);
            if (nd < dist[v]) { dist[v] = nd; prev[v] = u; }
        }
    }
    if (dist[to] >= 1.0e9f) return path;
    for (int v = to; v != -1; v = prev[v]) path.push_back(v);
    std::reverse(path.begin(), path.end());
    return path;
}

NavGraph NavGraph::createUniversity()
{
    using glm::vec3;
    NavGraph g;
    // ---- classroom, door, corridor (corridor lane is z = 6.0, kept free of furniture in Level.cpp)
    int classDoorIn  = g.addNode(vec3( 3.75f, 0.0f, 3.3f));    // 0 inside the classroom, next to the door
    int doorway      = g.addNode(vec3( 3.75f, 0.0f, 4.1f));    // 1 in the doorway (door gap is x 3.0 .. 4.2)
    int corrDoor     = g.addNode(vec3( 3.75f, 0.0f, 6.0f));    // 2 corridor, in front of the classroom door
    int corrWest     = g.addNode(vec3(-8.0f, 0.0f, 6.0f));     // 3 corridor, west part
    int corrEast     = g.addNode(vec3(10.5f, 0.0f, 6.0f));     // 4 corridor, east end
    // ---- staircase: flight A (north half) -> landing -> flight B (south half) -> upper hall
    int stairMouth   = g.addNode(vec3(-11.0f, 0.0f, 5.0f));    // 5 bottom of flight A
    int flightA      = g.addNode(vec3(-13.5f, 0.8f, 4.95f));   // 6 half-way up flight A
    int landing      = g.addNode(vec3(-16.8f, 1.6f, 5.7f));    // 7 the landing
    int flightB      = g.addNode(vec3(-13.9f, 2.4f, 6.45f));   // 8 half-way up flight B
    int upperTop     = g.addNode(vec3(-10.8f, 3.4f, 6.0f));    // 9 top of the stairs, upper floor
    int upperHall    = g.addNode(vec3( -8.8f, 3.4f, 5.7f));    // 10 inside the upper hall
    // ---- courtyard
    int exitDoor     = g.addNode(vec3( 3.75f, 0.0f, 7.3f));    // 11 the open exit door (south wall)
    int courtFront   = g.addNode(vec3( 3.7f, -0.02f, 10.5f));  // 12 in front of the building
    int courtCross   = g.addNode(vec3( 3.7f, -0.02f, 18.5f));  // 13 where the cross path meets the walkway
    int courtGate    = g.addNode(vec3( 3.7f, -0.02f, 28.0f));  // 14 near the gate
    int courtWest    = g.addNode(vec3(-8.0f, -0.02f, 18.5f));  // 15 west courtyard
    int courtEast    = g.addNode(vec3(13.0f, -0.02f, 18.5f));  // 16 east courtyard

    g.connect(classDoorIn, doorway);   g.connect(doorway, corrDoor);
    g.connect(corrDoor, corrWest);     g.connect(corrDoor, corrEast);
    g.connect(corrWest, stairMouth);   g.connect(stairMouth, flightA);
    g.connect(flightA, landing);       g.connect(landing, flightB);
    g.connect(flightB, upperTop);      g.connect(upperTop, upperHall);
    g.connect(corrDoor, exitDoor);     g.connect(exitDoor, courtFront);
    g.connect(courtFront, courtCross); g.connect(courtCross, courtGate);
    g.connect(courtCross, courtWest);  g.connect(courtCross, courtEast);
    (void)corrEast;
    return g;
}
