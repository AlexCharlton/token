//// Beta shape algorithm
//// Based heavily off of Beta-Shape Using Delaunay-Based Triangle Erosion by
//// Boiangiu and Zaharescu
//// http://www.wseas.us/e-library/conferences/2013/Budapest/MATH/MATH-14.pdf

#include <assert.h>

#define MAX_IMAGE_SIZE 400
#define MIN_EDGE_LEN 10.0

using namespace std;
using namespace cv;

typedef vector<Point> Contour;
typedef vector<Contour> Contours;
typedef vector<Vec4i> Hierarchy;
typedef pair<Point, Point> Edge;

struct Triangle{
    Point points[3];
    long int neighbours[3];

    Triangle(Point p1, Point p2, Point p3){
        points[0] = p1;
        points[1] = p2;
        points[2] = p3;
        neighbours[0] = -1;
        neighbours[1] = -1;
        neighbours[2] = -1;
    }

    void edge(size_t n, Edge &e) const{
        if (n >= 2) {
            e.first = points[2];
            e.second = points[0];
        } else {
            e.first = points[n];
            e.second = points[n+1];
        }
    }

    int contains_edge(Edge edge) const{
        for (int i = 0; i < 3; i++){
            if (edge.first == points[i]){
                for (int j = 0; j < 3; j++){
                    if (i == j) continue;
                    if (edge.second == points[j]){
                        if (abs(i - j) == 2) return 2;
                        if ((i == 0) or (j == 0)) return 0;
                        return 1;
                    }
                }
            }
        }
        return -1;
    }

    int n_neighbours() const{
        int n = 0;
        for (int i = 0; i < 3; i++){
            if (neighbours[i] >= 0) n++;
        }
        return n;
    }
};

typedef vector<Triangle> Triangles;

ostream& operator << (ostream &o, const Triangle &t){
    o << "{" << t.points[0] << ", " << t.points[1] << ", " << t.points[2] << "}<<" << t.neighbours[0] << "," << t.neighbours[1] << "," << t.neighbours[2] << ">>";
    return o;
}

float edge_length(Edge &e){
    float x = e.first.x - e.second.x;
    float y = e.first.y - e.second.y;
    return sqrt(x*x + y*y);
}

struct OuterEdge{
    size_t triangle, edge;
    float len;
    bool resolved_p;

    OuterEdge(size_t t, size_t e, const Triangles &tris){
        triangle = t;
        edge = e;
        Edge ed; tris[t].edge(e, ed);
        len = edge_length(ed);
        resolved_p = (tris[t].n_neighbours() < 2) || (len < MIN_EDGE_LEN);
    }

    void check_resolved(const Triangles &tris){
        if ((tris[triangle].n_neighbours() < 2) || 
            (len < MIN_EDGE_LEN)){
            resolved_p = true;
        }
    }
};

typedef vector<OuterEdge> OuterEdges;

bool out_of_range_p(Point p){
    return (abs(p.x) > MAX_IMAGE_SIZE) || (abs(p.y) > MAX_IMAGE_SIZE);
}

int get_triangles(const Contours &contours, Triangles &triangles){
    // Build Subdiv
    Rect rect(0, 0, 400, 400);
    Subdiv2D subdiv(rect);
    vector<Point2f> points;
    for(size_t i = 0; i < contours.size(); i++){
        for(size_t j = 0; j < contours[i].size(); j++){
            points.push_back(Point2f(contours[i][j].x, contours[i][j].y));
        }
    }
    if (points.size() < 3) return 0;
    subdiv.insert(points);
    // Initialize triangles
    vector<Vec6f> triangleList;
    subdiv.getTriangleList(triangleList);
    for(size_t i = 0; i < triangleList.size(); i++){
        Vec6f t = triangleList[i];
        Point pts[3];
        pts[0] = Point(cvRound(t[0]), cvRound(t[1]));
        pts[1] = Point(cvRound(t[2]), cvRound(t[3]));
        pts[2] = Point(cvRound(t[4]), cvRound(t[5]));
        if (out_of_range_p(pts[0]) || out_of_range_p(pts[1]) || 
            out_of_range_p(pts[2])) continue;
        triangles.emplace_back(pts[0], pts[1], pts[2]);
    }
    // Get neighbours
    for(size_t i = 0; i < triangles.size(); i++){
        for(size_t e = 0; e < 3; e++){
            if (triangles[i].neighbours[e] >= 0) 
                continue; // neighbour already found
            Edge edge; triangles[i].edge(e, edge);
            for(size_t j = 0; j < triangles.size(); j++){
                if (j == i) continue;
                int d = triangles[j].contains_edge(edge);
                if (d >= 0){
                    triangles[i].neighbours[e] = j;
                    triangles[j].neighbours[d] = i;
                    break;
                }
            }
        }
    }
    return 1;
}

void get_outer_edges(const Triangles &tris, OuterEdges &edges){
    for(size_t t = 0; t < tris.size(); t++){
        for(size_t e = 0; e < 3; e++){
            if (tris[t].neighbours[e] < 0){
                edges.emplace_back(t, e, tris);
            }
        }
    }
}

Point shared_point(Edge a, Edge b){
    if ((a.first == b.first) || (a.first == b.second)) return a.first;
    return a.second;
}

bool point_in_edge_p(Point p, Edge e){
    return (p == e.first) || (p == e.second);
}

bool in_outer_edges_p(Point p, const OuterEdges &edges, const Triangles &tris){
    for(size_t e = 0; e < edges.size(); e++){
        Edge edge;
        tris[edges[e].triangle].edge(edges[e].edge, edge);
        if (point_in_edge_p(p, edge)) return true;
    }
    return false;
}

// void print_edge(int e,  const OuterEdges edges, const Triangles tris){
//     OuterEdge edge = edges[e];
//     cout << edge.edge << ": " << tris[edge.triangle] << " -- len: " << edge.len << " resolved: " << edge.resolved_p << endl;
// }

void get_contour(const OuterEdges &edges, const Triangles &tris, Contour &c){
    Edge first, last, edge;
    tris[edges[0].triangle].edge(edges[0].edge, last);
    first = last;
    c.push_back(first.first);
    while(true){
        for(size_t e = 0; e < edges.size(); e++){
            tris[edges[e].triangle].edge(edges[e].edge, edge);
            if(edge.first == last.second){
                last = edge;
                c.push_back(edge.first);
                break;
            }
            assert("Couldn't find a matching edge.");
        }
        if (last.second == first.first) break;
    }
}

void get_beta_shape(const Contours &contours, Contour &beta){
    Triangles triangles;
    if (!get_triangles(contours, triangles)) return;
    OuterEdges edges;
    get_outer_edges(triangles, edges);

    while(true){
        int edge = -1;
        float max = -1.0;
        for(size_t e = 0; e < edges.size(); e++){
            edges[e].check_resolved(triangles);
            if ((edges[e].len > max) && !edges[e].resolved_p){
                max = edges[e].len;
                edge = e;
            }
        }
        if (edge < 0) break;
        OuterEdge e = edges[edge];
        Edge edj1, edj2;
        int n1t, n1e, n2t, n2e, n1 , n2; // neighbouring triangles
        n1 = (e.edge == 0) ? 2 : e.edge - 1;
        n2 = (e.edge == 2) ? 0 : e.edge + 1;
        triangles[e.triangle].edge(n1, edj1);
        triangles[e.triangle].edge(n2, edj2);
        n1t = triangles[e.triangle].neighbours[n1];
        n1e = triangles[n1t].contains_edge(edj1);
        n2t = triangles[e.triangle].neighbours[n2];
        n2e = triangles[n2t].contains_edge(edj2);
        assert ((n1t >= 0) && (n1e >= 0) && (n2t >= 0) && (n2e >= 0));
        Edge n1edge, n2edge;
        triangles[n1t].edge(n1e, n1edge);
        triangles[n2t].edge(n2e, n2edge);
        Point shared = shared_point(n1edge, n2edge);
        if (in_outer_edges_p(shared, edges, triangles)){
            // Removing this edge breaks the contour
            edges[edge].resolved_p = true;
        } else {
            edges.erase(edges.begin() + edge);
            triangles[n1t].neighbours[n1e] = -1;
            triangles[n2t].neighbours[n2e] = -1;
            edges.emplace_back(n1t, n1e, triangles);
            edges.emplace_back(n2t, n2e, triangles);
        }
    }
    Contour c;
    get_contour(edges, triangles, c);
    approxPolyDP(c, beta, MIN_EDGE_LEN/2, true);
}
