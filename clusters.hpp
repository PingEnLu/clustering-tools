#ifndef CLUSTERS_HPP
#define CLUSTERS_HPP
#include "containers/VectorSet.hpp"
#include "containers/disjoint_sets.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <list>
#include <random>
#include <set>
#include <tuple>
#include <vector>

class Clusters {
  public:
    enum InputFormat { TWO_COLOUMN, THREE_COLOUMN };
    enum PartitionMethod { KSETS_PLUS, PARTITION_INPUT };
    class NodeInfo {
      public:
        size_t vid;
        double weight;
        NodeInfo(size_t _vid, double _weight) : vid(_vid), weight(_weight) {}
        NodeInfo(size_t _vid) : NodeInfo(_vid, 0.0) {}
    };

    class NeighborVertex {
      public:
        NeighborVertex(size_t _cid) : cid(_cid), cross(0.0) {}
        size_t cid;
        double cross;
    };

    template <typename T> class VertexRecord {
      public:
        size_t vid;
        T it;
        VertexRecord(size_t _vid) : vid(_vid) {}
        inline bool is_recorded_by(size_t _vid) { return vid == _vid; }
        inline void record(size_t _vid, T _it) {
            vid = _vid;
            it = _it;
        }
    };
    size_t original_vcount;
    size_t vcount;
    size_t k;
    std::vector<std::list<NodeInfo>> adj_list;
    std::vector<double> pv_list;
    std::vector<double> pc_list;
    std::vector<double> pcc_list;
    std::vector<double> pvv_list;
    std::vector<size_t> which_supernode;
    VectorSet nonempty_set;
    DisjointSets sets;
    unsigned seed;
    double total_weight;
    std::list<size_t> size_record;
    std::list<size_t> iter_record;

    Clusters(size_t _vcount, size_t _k, std::istream &file,
             InputFormat inputformat = TWO_COLOUMN)
        : original_vcount(_vcount), vcount(_vcount), k(_k), adj_list(_vcount),
          pv_list(_vcount, 0.0), pc_list(_k, 0.0), pcc_list(_k, 0.0),
          pvv_list(_vcount, 0.0), which_supernode(_vcount, 0), nonempty_set(_k),
          sets(_vcount, _k), total_weight(0.0) {
        // seed(std::chrono::system_clock::now().time_since_epoch().count())
        for (size_t cid = 0; cid < k; cid++) {
            nonempty_set.insert(cid);
        }
        read_weighted_edgelist_undirected(file, inputformat);
    }

    void add_edge(const size_t &vid1, const size_t &vid2,
                  const double &weight) {
        total_weight += weight;
        adj_list[vid1].push_back(NodeInfo(vid2, weight));
        adj_list[vid2].push_back(NodeInfo(vid1, weight));
        pv_list[vid1] += weight;
        pv_list[vid2] += weight;
        size_t cid1 = sets.which_cluster[vid1];
        size_t cid2 = sets.which_cluster[vid2];
        pc_list[cid1] += weight;
        pc_list[cid2] += weight;
        if (cid1 == cid2) {
            pcc_list[cid1] += weight;
        }
    }
    void read_weighted_edgelist_undirected(std::istream &file,
                                           InputFormat inputformat) {
        size_t vid1, vid2;

        if (inputformat == TWO_COLOUMN) {
            while (file >> vid1 >> vid2) {
                add_edge(vid1, vid2, 1);
            }
        } else if (inputformat == THREE_COLOUMN) {
            double weight;
            while (file >> vid1 >> vid2 >> weight) {
                add_edge(vid1, vid2, weight);
            }
        }

        for (auto &pv : pv_list) {
            pv /= 2 * total_weight;
        }
        for (auto &pc : pc_list) {
            pc /= 2 * total_weight;
        }
        for (auto &pcc : pcc_list) {
            pcc /= 2 * total_weight;
        }
        for (size_t vid = 0; vid < vcount; vid++) {
            which_supernode[vid] = vid;
        }
    }

    bool partition_procedure(const PartitionMethod &method) {
        size_t round_count = 0;
        bool changed_once = false;
        bool changed = true;

        while (changed) {
            round_count++;
            changed = false;
            std::vector<
                VertexRecord<typename std::list<NeighborVertex>::iterator>>
                candidate_sets(vcount, vcount);
            for (size_t vid = 0; vid < vcount; vid++) {
                size_t old_cid = sets.which_cluster[vid];
                if (sets.size[old_cid] <= 1 &&
                    method == PartitionMethod::KSETS_PLUS) {
                    continue;
                }
                std::list<NeighborVertex> candidate_list;
                // old cid should be listed on the first of the list
                candidate_sets[old_cid] = vid;
                candidate_list.push_back(NeighborVertex(old_cid));
                candidate_sets[old_cid].it = std::prev(candidate_list.end());

                for (auto &vertex2 : adj_list[vid]) {
                    size_t vid2 = vertex2.vid;
                    size_t cid = sets.which_cluster[vid2];
                    typename std::list<NeighborVertex>::iterator it;
                    if (!candidate_sets[cid].is_recorded_by(vid)) {
                        candidate_list.push_back(NeighborVertex(cid));
                        candidate_sets[cid].record(
                            vid, std::prev(candidate_list.end()));
                    }
                    candidate_sets[cid].it->cross += vertex2.weight;
                }
                for (auto &neighbor : candidate_list) {
                    neighbor.cross /= 2 * total_weight;
                }

                size_t best_cid = old_cid;
                double old_cross = candidate_list.begin()->cross;
                double best_cross = old_cross;
                // for the case chosing ksets+
                double best_correlation_measure;
                if (method == PartitionMethod::KSETS_PLUS) {

                } else {
                    best_correlation_measure =
                        -std::numeric_limits<double>::max(); // best modularity
                                                             // = -inf
                    for (auto &neighbor : candidate_list) {
                        size_t cid = neighbor.cid;
                        double correlation_measure = neighbor.cross;

                        if (old_cid == cid) {
                            if (sets.size[old_cid] == 1) {
                                correlation_measure = 0;
                            } else {
                                correlation_measure -=
                                    (pc_list[cid] - pv_list[vid]) *
                                    pv_list[vid];
                            }
                        } else {
                            correlation_measure -= pc_list[cid] * pv_list[vid];
                        }
                        if (correlation_measure > best_correlation_measure) {
                            best_correlation_measure = correlation_measure;
                            best_cid = cid;
                            best_cross = neighbor.cross;
                        }
                    }
                }

                // doesn't change
                if (best_cid == old_cid) {
                    continue;
                }

                if (method == PartitionMethod::PARTITION_INPUT &&
                    sets.size[old_cid] == 1) {
                    nonempty_set.erase(old_cid);
                }

                changed = true;
                changed_once = true;
                sets.move(vid, best_cid);

                // P{uniform choice an edge, and one end is in cluster c}
                pc_list[old_cid] -= pv_list[vid];
                pc_list[best_cid] += pv_list[vid];

                // P{uniform choice and edge, and two ends are in cluster c}
                pcc_list[old_cid] -= 2 * old_cross + pvv_list[vid];
                pcc_list[best_cid] += 2 * best_cross + pvv_list[vid];
            }
        }
        iter_record.push_back(round_count);
        return changed_once;
    }
    bool NodeAggregate() { // O(m+n)
        size_t new_vcount = nonempty_set.size();
        VectorSet new_nonempty_set(new_vcount);
        size_t max_cid = vcount;
        std::vector<std::list<NodeInfo>> new_adj_list(new_vcount);
        std::vector<double> new_pv_list(new_vcount, 0.0);
        std::vector<double> new_pcc_list(new_vcount, 0.0);
        typedef std::list<NodeInfo>::iterator It;
        std::vector<VertexRecord<std::pair<It, It>>> candidate_sets(max_cid,
                                                                    max_cid);
        class Mapping {
          public:
            std::vector<size_t> mapping;
            const size_t NONE;
            size_t id_pivot;
            Mapping(size_t vcount)
                : mapping(vcount, vcount), NONE(vcount), id_pivot(0) {}
            size_t &operator[](size_t old_id) {
                if (mapping[old_id] == NONE) {
                    mapping[old_id] = id_pivot;
                    id_pivot++;
                }
                return mapping[old_id];
            }
            size_t size() { return id_pivot; }
        };

        Mapping mapping(max_cid);
        size_t cid_pivot = 0;
        for (auto &cid1 : nonempty_set) {
            size_t new_vid1 = mapping[cid1];
            size_t new_cid1 = new_vid1;
            new_nonempty_set.insert(new_cid1);
            new_pcc_list[new_cid1] = pcc_list[cid1];
            for (auto vid1 = sets.begin(cid1); vid1 != sets.end();
                 vid1 = sets.next(vid1)) {
                new_pv_list[new_vid1] += pv_list[vid1];
                for (auto &vertex2 : adj_list[vid1]) {
                    size_t vid2 = vertex2.vid;
                    size_t cid2 = sets.which_cluster[vid2];
                    size_t new_vid2 = mapping[cid2];
                    // !(vid1 >= vid2) -> avoid duplicate calculation
                    // !(new_vid1 == new_vid2) -> same cluster
                    if (new_vid1 >= new_vid2) {
                        continue;
                    }

                    double weight = vertex2.weight;
                    // different cluster
                    std::pair<It, It> it;
                    if (!candidate_sets[new_vid2].is_recorded_by(new_vid1)) {
                        new_adj_list[new_vid1].push_back(NodeInfo(new_vid2));
                        new_adj_list[new_vid2].push_back(NodeInfo(new_vid1));
                        std::get<0>(it) =
                            std::prev(new_adj_list[new_vid1].end());
                        std::get<1>(it) =
                            std::prev(new_adj_list[new_vid2].end());
                        candidate_sets[new_vid2].record(new_vid1, it);
                    }
                    it = candidate_sets[new_vid2].it;
                    std::get<0>(it)->weight += weight;
                    std::get<1>(it)->weight += weight;
                }
            }
        }
        std::vector<double> new_pc_list(new_pv_list);

        std::vector<size_t> new_which_cluster(new_vcount);
        for (size_t vid = 0; vid < new_vcount; vid++) {
            new_which_cluster[vid] = vid;
        }
        pv_list = std::move(new_pv_list);
        pc_list = std::move(new_pc_list);
        pcc_list = std::move(new_pcc_list);
        pvv_list = pcc_list;
        adj_list = std::move(new_adj_list);
        vcount = new_vcount;
        DisjointSets new_sets(new_vcount, new_vcount, new_which_cluster.begin(),
                              new_which_cluster.end());

        for (size_t vid = 0; vid < original_vcount; vid++) {
            size_t supernode_id = which_supernode[vid];
            size_t cid = sets.which_cluster[supernode_id];
            which_supernode[vid] = mapping[cid];
        }
        sets = std::move(new_sets);
        nonempty_set = std::move(new_nonempty_set);
        size_record.push_back(nonempty_set.size());
        return true;
    }

    void routine() {
        while (true) {
            k = vcount;
            if (!partition_procedure(PartitionMethod::PARTITION_INPUT) ||
                !NodeAggregate()) {
                break;
            }
        }
    }

    void print() {
        for (size_t vid = 0; vid < original_vcount; vid++) {
            std::cout << sets.which_cluster[which_supernode[vid]] << " ";
        }
        std::cout << std::endl;
    }
    void print_communities() {

        DisjointSets s(original_vcount, nonempty_set.size(),
                       which_supernode.begin(), which_supernode.end());
        s.print();
    }
    void print_size(std::ostream &f) {
        for (auto &v : size_record) {
            f << v << " ";
        }
        f << std::endl;
    }
    void print_iter(std::ostream &f) {
        for (auto &v : iter_record) {
            f << v << " ";
        }
        f << std::endl;
    }
};

#endif
