//***************************************************************************
//* Copyright (c) 2011-2013 Saint-Petersburg Academic University
//* All Rights Reserved
//* See file LICENSE for details.
//****************************************************************************

#pragma once

#include "statistics.hpp"
#include "debruijn_graph.hpp"

#include "graph_pack.hpp"
#include "graphio.hpp"

#include "omni/visualization/visualization.hpp"
#include "omni/edges_position_handler.hpp"
#include "omni/graph_component.hpp"
#include "io/rc_reader_wrapper.hpp"
#include "io/delegating_reader_wrapper.hpp"
#include "io/easy_reader.hpp"
#include "io/splitting_wrapper.hpp"
#include "io/wrapper_collection.hpp"
#include "io/osequencestream.hpp"
#include "dataset_readers.hpp"
#include "copy_file.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <cmath>

namespace debruijn_graph {

template<class Graph, class Index>
class GenomeMappingStat: public AbstractStatCounter {
  private:
    typedef typename Graph::EdgeId EdgeId;
    const Graph &graph_;
    const Index& index_;
    Sequence genome_;
    size_t k_;
  public:
    GenomeMappingStat(const Graph &graph, const Index &index,	Sequence genome, size_t k) :
            graph_(graph), index_(index), genome_(genome), k_(k) {}

    virtual ~GenomeMappingStat() {}

    virtual void Count() {
        INFO("Mapping genome");
        size_t break_number = 0;
        size_t covered_kp1mers = 0;
        size_t fail = 0;
        if (genome_.size() <= k_)
            return;

        runtime_k::RtSeq cur = genome_.start<runtime_k::RtSeq>(k_ + 1);
        cur >>= 0;
        bool breaked = true;
        pair<EdgeId, size_t> cur_position;
        for (size_t cur_nucl = k_; cur_nucl < genome_.size(); cur_nucl++) {
            cur <<= genome_[cur_nucl];
            if (index_.contains(cur)) {
                pair<EdgeId, size_t> next = index_.get(cur);
                if (!breaked
                    && cur_position.second + 1
                    < graph_.length(cur_position.first)) {
                    if (next.first != cur_position.first
                        || cur_position.second + 1 != next.second) {
                        fail++;
                    }
                }
                cur_position = next;
                covered_kp1mers++;
                breaked = false;
            } else {
                if (!breaked) {
                    breaked = true;
                    break_number++;
                }
            }
        }
        INFO("Genome mapped");
        INFO("Genome mapping results:");
        INFO("Covered k+1-mers:" << covered_kp1mers << " of " << (genome_.size() - k_) << " which is "
             << (100.0 * (double) covered_kp1mers / (double) (genome_.size() - k_)) << "%");
        INFO("Covered k+1-mers form " << break_number + 1 << " contigious parts");
        INFO("Continuity failtures " << fail);
    }
};

template<class Graph, class Index>
class StatCounter: public AbstractStatCounter {
  private:
    StatList stats_;
  public:
    typedef typename Graph::VertexId VertexId;
    typedef typename Graph::EdgeId EdgeId;

    StatCounter(const Graph& graph, const Index& index,
                const Sequence& genome, size_t k) {
        SimpleSequenceMapper<Graph, Index> sequence_mapper(graph, index, k + 1);
        Path<EdgeId> path1 = sequence_mapper.MapSequence(Sequence(genome));
        Path<EdgeId> path2 = sequence_mapper.MapSequence(!Sequence(genome));
        stats_.AddStat(new VertexEdgeStat<Graph>(graph));
        stats_.AddStat(new BlackEdgesStat<Graph>(graph, path1, path2));
        stats_.AddStat(new NStat<Graph>(graph, path1, 50));
        stats_.AddStat(new SelfComplementStat<Graph>(graph));
        stats_.AddStat(
            new GenomeMappingStat<Graph, Index>(graph, index,
                                                Sequence(genome), k));
        stats_.AddStat(new IsolatedEdgesStat<Graph>(graph, path1, path2));
    }

    virtual ~StatCounter() {
        stats_.DeleteStats();
    }

    virtual void Count() {
        stats_.Count();
    }

  private:
    DECL_LOGGER("StatCounter")
};

template<class Graph, class Index>
void CountStats(const Graph& g, const Index& index,
                const Sequence& genome, size_t k) {
    INFO("Counting stats");
    StatCounter<Graph, Index> stat(g, index, genome, k);
    stat.Count();
    INFO("Stats counted");
}

template<class Graph>
void WriteErrorLoc(const Graph &g,
                   const string& folder_name,
                   std::shared_ptr<omnigraph::visualization::GraphColorer<Graph>> genome_colorer,
                   const omnigraph::GraphLabeler<Graph>& labeler) {
    INFO("Writing error localities for graph to folder " << folder_name);
    GraphComponent<Graph> all(g, g.begin(), g.end());
    set<EdgeId> edges = genome_colorer->ColoredWith(all.edges().begin(),
                                                    all.edges().end(), "black");
    set<typename Graph::VertexId> to_draw;
    for (auto it = edges.begin(); it != edges.end(); ++it) {
        to_draw.insert(g.EdgeEnd(*it));
        to_draw.insert(g.EdgeStart(*it));
    }
    shared_ptr<GraphSplitter<Graph>> splitter = StandardSplitter(g, to_draw);
    WriteComponents(g, folder_name, splitter, genome_colorer, labeler);
    INFO("Error localities written written to folder " << folder_name);
}

template<class Graph, class Index>
Path<typename Graph::EdgeId> FindGenomePath(const Sequence& genome,
                                            const Graph& g, const Index& index, size_t k) {
    SimpleSequenceMapper<Graph, Index> srt(g, index, k + 1);
    return srt.MapSequence(genome);
}

template<class Graph, class Index>
MappingPath<typename Graph::EdgeId>
FindGenomeMappingPath(const Sequence& genome, const Graph& g,
                      const Index& index,
                      const KmerMapper<Graph>& kmer_mapper) {
    NewExtendedSequenceMapper<Graph, Index> srt(g, index, kmer_mapper, g.k() + 1);
    return srt.MapSequence(genome);
}

template<class Graph>
void WriteGraphComponentsAlongGenome(const Graph& g,
                                     const GraphLabeler<Graph>& labeler,
                                     const string& folder,
                                     const Path<typename Graph::EdgeId>& path1,
                                     const Path<typename Graph::EdgeId>& path2) {
    INFO("Writing graph components along genome");

    make_dir(folder);
    omnigraph::visualization::WriteComponentsAlongPath(g, path1, folder, omnigraph::visualization::DefaultColorer(g, path1, path2), labeler);

    INFO("Writing graph components along genome finished");
}

//todo refactoring needed: use graph pack instead!!!
template<class Graph, class Mapper>
void WriteGraphComponentsAlongContigs(const Graph& g,
                                      Mapper &mapper,
                                      const std::string& folder,
                                      std::shared_ptr<omnigraph::visualization::GraphColorer<Graph>> colorer,
                                      const GraphLabeler<Graph>& labeler) {
    INFO("Writing graph components along contigs");
    io::EasyReader contigs_to_thread(cfg::get().pos.contigs_to_analyze, false/*true*/);
    contigs_to_thread.reset();
    io::SingleRead read;
    while (!contigs_to_thread.eof()) {
        contigs_to_thread >> read;
        make_dir(folder + read.name());
        omnigraph::visualization::WriteComponentsAlongPath(g, mapper.MapSequence(read.sequence()).simple_path(), folder + read.name() + "/",
                                                           colorer, labeler);
    }
    INFO("Writing graph components along contigs finished");
}

template<class Graph>
void WriteKmerComponent(conj_graph_pack &gp, runtime_k::RtSeq const& kp1mer, const std::string& file,
                        std::shared_ptr<omnigraph::visualization::GraphColorer<Graph>> colorer,
                        const omnigraph::GraphLabeler<Graph>& labeler) {
    if(!gp.index.contains(kp1mer)) {
        WARN("no such kmer in the graph");
        return;
    }
    VERIFY(gp.index.contains(kp1mer));
    auto pos = gp.index.get(kp1mer);
    VertexId v = pos.second * 2 < gp.g.length(pos.first) ? gp.g.EdgeStart(pos.first) : gp.g.EdgeEnd(pos.first);
    GraphComponent<Graph> component = omnigraph::VertexNeighborhood<Graph>(gp.g, v);
    omnigraph::visualization::WriteComponent<Graph>(component, file, colorer, labeler);
}

inline
optional<runtime_k::RtSeq> FindCloseKP1mer(const conj_graph_pack &gp,
                                           size_t genome_pos, size_t k) {
    VERIFY(gp.genome.size() > 0);
    VERIFY(genome_pos < gp.genome.size());
    static const size_t magic_const = 200;
    for (size_t diff = 0; diff < magic_const; diff++) {
        for (int dir = -1; dir <= 1; dir += 2) {
            size_t pos = (gp.genome.size() - k + genome_pos + dir * diff) % (gp.genome.size() - k);
            cout << pos << endl;
            runtime_k::RtSeq kp1mer = gp.kmer_mapper.Substitute(
                runtime_k::RtSeq (k + 1, gp.genome, pos));
            cout << "oppa" << endl;
            if (gp.index.contains(kp1mer))
                return optional<runtime_k::RtSeq>(kp1mer);
        }
    }
    return none;
}

template<class Graph>
void ProduceDetailedInfo(conj_graph_pack &gp,
                         const omnigraph::GraphLabeler<Graph>& labeler, const string& run_folder,
                         const string &pos_name,
                         info_printer_pos pos,
                         size_t k) {
    string base_folder = path::append_path(run_folder, "pictures/");
    make_dir(base_folder);
    string folder = path::append_path(base_folder, pos_name + "/");

    auto it = cfg::get().info_printers.find(pos);
    VERIFY(it != cfg::get().info_printers.end());

    const debruijn_config::info_printer & config = it->second;


    if (config.print_stats) {
        INFO("Printing statistics for " << details::info_printer_pos_name(pos));
        CountStats(gp.g, gp.index, gp.genome, k);
    }

    typedef Path<typename Graph::EdgeId> path_t;
    path_t path1;
    path_t path2;
    shared_ptr<omnigraph::visualization::GraphColorer<Graph>> colorer = omnigraph::visualization::DefaultColorer(gp.g);

    if (config.write_error_loc ||
        config.write_full_graph ||
        config.write_full_nc_graph ||
        config.write_components ||
        !config.components_for_kmer.empty() ||
        config.write_components_along_genome ||
        config.write_components_along_contigs || config.save_full_graph ||
        !config.components_for_genome_pos.empty()) {
        path1 = FindGenomeMappingPath(gp.genome, gp.g, gp.index,
                                      gp.kmer_mapper).simple_path();
        path2 = FindGenomeMappingPath(!gp.genome, gp.g, gp.index,
                                      gp.kmer_mapper).simple_path();
        colorer = omnigraph::visualization::DefaultColorer(gp.g, path1, path2);
        //		path1 = FindGenomePath<K>(gp.genome, gp.g, gp.index);
        //		path2 = FindGenomePath<K>(!gp.genome, gp.g, gp.index);
        make_dir(folder);
    }

    if (config.write_error_loc) {
        make_dir(folder + "error_loc/");
        WriteErrorLoc(gp.g, folder + "error_loc/", colorer, labeler);
    }

    if (config.write_full_graph) {
        WriteComponent(GraphComponent<Graph>(gp.g, gp.g.begin(), gp.g.end()), folder + "full_graph.dot", colorer, labeler);
    }

    if (config.write_full_nc_graph) {
        WriteSimpleComponent(GraphComponent<Graph>(gp.g, gp.g.begin(), gp.g.end()), folder + "nc_full_graph.dot", colorer, labeler);
    }

    if (config.write_components) {
        make_dir(folder + "components/");
        omnigraph::visualization::WriteComponents(gp.g, folder + "components/", omnigraph::ReliableSplitter<Graph>(gp.g), colorer, labeler);
    }

    if (!config.components_for_kmer.empty()) {
        string kmer_folder = path::append_path(base_folder, "kmer_loc/");
        make_dir(kmer_folder);
        auto kmer = runtime_k::RtSeq(k + 1, config.components_for_kmer.substr(0, k + 1).c_str());
        string file_name = path::append_path(kmer_folder, pos_name + ".dot");
        WriteKmerComponent(gp, kmer, file_name, colorer,labeler);
    }

    if (config.write_components_along_genome) {
        make_dir(folder + "along_genome/");
        omnigraph::visualization::WriteComponentsAlongPath(gp.g, path1, folder + "along_genome/", colorer, labeler);
    }

    if (config.write_components_along_contigs) {
        make_dir(folder + "along_contigs/");
        NewExtendedSequenceMapper<Graph, Index> mapper(gp.g, gp.index, gp.kmer_mapper, gp.g.k() + 1);
        WriteGraphComponentsAlongContigs(gp.g, mapper, folder + "along_contigs/", colorer, labeler);
    }

    if (config.save_full_graph) {
        make_dir(folder + "full_graph_save/");
        graphio::PrintGraphPack(folder + "full_graph_save/graph", gp);
    }

    if (!config.components_for_genome_pos.empty()) {
        string pos_loc_folder = path::append_path(base_folder, "pos_loc/");
        make_dir(pos_loc_folder);
        vector<string> positions;
        boost::split(positions, config.components_for_genome_pos,
                     boost::is_any_of(" ,"), boost::token_compress_on);
        for (auto it = positions.begin(); it != positions.end(); ++it) {
            optional < runtime_k::RtSeq > close_kp1mer = FindCloseKP1mer(gp,
                                                                         boost::lexical_cast<int>(*it), k);
            if (close_kp1mer) {
                string locality_folder = path::append_path(pos_loc_folder, *it + "/");
                make_dir(locality_folder);
                WriteKmerComponent(gp, *close_kp1mer, path::append_path(locality_folder, pos_name + ".dot"), colorer, labeler);
            } else {
                WARN(
                    "Failed to find genome kp1mer close to the one at position "
                    << *it << " in the graph. Which is " << runtime_k::RtSeq (k + 1, gp.genome, boost::lexical_cast<int>(*it)));
            }
        }
    }
}

struct detail_info_printer {
    detail_info_printer(conj_graph_pack &gp,
                        const omnigraph::GraphLabeler<Graph>& labeler, const string& folder)
            :  folder_(folder),
               func_(bind(&ProduceDetailedInfo<conj_graph_pack::graph_t>, boost::ref(gp),
                     boost::ref(labeler), _3, _2, _1, gp.k_value)),
               graph_(gp.g), cnt(0) {
    }

    void operator()(info_printer_pos pos,
                    string const& folder_suffix = "") {
        cnt++;
        string pos_name = details::info_printer_pos_name(pos);
        VertexEdgeStat<conj_graph_pack::graph_t> stats(graph_);
        TRACE("Number of vertices : " << stats.vertices() << ", number of edges : " << stats.edges() << ", sum length of edges : " << stats.edge_length());
        func_(pos,
              ToString(cnt, 2) + "_" + pos_name + folder_suffix,
              folder_
            //                (path::append_path(folder_, (pos_name + folder_suffix)) + "/")
              );
    }

  private:
    string folder_;
    boost::function<void(info_printer_pos, string const&, string const&)> func_;
    const conj_graph_pack::graph_t &graph_;
    size_t cnt;
};

inline
std::string ConstructComponentName(std::string file_name, size_t cnt) {
    stringstream ss;
    ss << cnt;
    string res = file_name;
    res.insert(res.length(), ss.str());
    return res;
}

template<class Graph>
double AvgCoverage(const Graph& g,
                   const std::vector<typename Graph::EdgeId>& edges) {
    double total_cov = 0.;
    size_t total_length = 0;
    for (auto it = edges.begin(); it != edges.end(); ++it) {
        total_cov += g.coverage(*it) * (double) g.length(*it);
        total_length += g.length(*it);
    }
    return total_cov / (double) total_length;
}

template<class Graph, class Mapper>
class PosFiller {
    typedef typename Graph::EdgeId EdgeId;
    const Graph& g_;
    const Mapper& mapper_;
    EdgesPositionHandler<Graph>& edge_pos_;

  public:
    PosFiller(const Graph& g, const Mapper& mapper,
              EdgesPositionHandler<Graph>& edge_pos) :
            g_(g), mapper_(mapper), edge_pos_(edge_pos) {

    }

    void Process(const Sequence& s, string name) const {
        //todo stupid conversion!

        return Process(io::SingleRead(name, s.str()));
    }

    void Process(const io::SingleRead& read) const {
        MappingPath<EdgeId> path = mapper_.MapRead(read);
        const string& name = read.name();
        int cur_pos = 0;
        TRACE(
            "Contig " << name << " mapped on " << path.size()
            << " fragments.");
        for (size_t i = 0; i < path.size(); i++) {
            EdgeId ei = path[i].first;
            MappingRange mr = path[i].second;
            int len = (int) (mr.mapped_range.end_pos - mr.mapped_range.start_pos);
            if (i > 0)
                if (path[i - 1].first != ei)
                    if (g_.EdgeStart(ei) != g_.EdgeEnd(path[i - 1].first)) {
                        TRACE(
                            "Contig " << name
                            << " mapped on not adjacent edge. Position in contig is "
                            << path[i - 1].second.initial_range.start_pos
                            + 1
                            << "--"
                            << path[i - 1].second.initial_range.end_pos
                            << " and "
                            << mr.initial_range.start_pos + 1
                            << "--" << mr.initial_range.end_pos);
                    }
            edge_pos_.AddEdgePosition(ei, (int) mr.initial_range.start_pos + 1,
                                      (int) mr.initial_range.end_pos, name,
                                      (int) mr.mapped_range.start_pos + 1,
                                      (int) mr.mapped_range.end_pos);
            cur_pos += len;
        }
    }

  private:
    DECL_LOGGER("PosFiller")
    ;
};

template<class Graph, class Mapper>
void FillPos(const Graph& g, const Mapper& mapper,
             EdgesPositionHandler<Graph>& edge_pos,
             io::IReader<io::SingleRead>& stream) {
    PosFiller<Graph, Mapper> filler(g, mapper, edge_pos);
    io::SingleRead read;
    while (!stream.eof()) {
        stream >> read;
        filler.Process(read);
    }
}

template<class gp_t>
void FillPos(gp_t& gp, io::IReader<io::SingleRead>& stream) {
    FillPos(gp.g, *MapperInstance(gp), gp.edge_pos, stream);
}

template<class Graph, class Mapper>
void FillPos(const Graph& g, const Mapper& mapper, EdgesPositionHandler<Graph>& edge_pos, const Sequence& s, const string& name) {
    PosFiller<Graph, Mapper>(g, mapper, edge_pos).Process(s, name);
}

template<class gp_t>
void FillPos(gp_t& gp, const Sequence& s, const string& name) {
    FillPos(gp.g, *MapperInstance(gp), gp.edge_pos, s, name);
}

//deprecated, todo remove usages!!!
template<class gp_t>
void FillPos(gp_t& gp, const string& contig_file, string prefix) {
    //	typedef typename gp_t::Graph::EdgeId EdgeId;
    INFO("Threading large contigs");
    io::Reader irs(contig_file);
    while(!irs.eof()) {
        io::SingleRead read;
        irs >> read;
        DEBUG("Contig " << read.name() << ", length: " << read.size());
        if (!read.IsValid()) {
            WARN("Attention: contig " << read.name() << " contains Ns");
            continue;
        }
        Sequence contig = read.sequence();
        if (contig.size() < 1500000) {
            //		continue;
        }
        FillPos(gp, contig, prefix + read.name());
    }
}

template<class gp_t>
void FillPosWithRC(gp_t& gp, const string& contig_file, string prefix) {
    //  typedef typename gp_t::Graph::EdgeId EdgeId;
    INFO("Threading large contigs");
    io::EasySplittingReader irs(contig_file, true);
    while(!irs.eof()) {
        io::SingleRead read;
        irs >> read;
        DEBUG("Contig " << read.name() << ", length: " << read.size());
        if (!read.IsValid()) {
            WARN("Attention: contig " << read.name()
                 << " is not valid (possibly contains N's)");
            continue;
        }
        Sequence contig = read.sequence();
        FillPos(gp, contig, prefix + read.name());
    }
}

template<class Graph>
size_t Nx(Graph &g, double percent) {
    size_t sum_edge_length = 0;
    vector<size_t> lengths;
    for (auto iterator = g.ConstEdgeBegin(); !iterator.IsEnd(); ++iterator) {
        lengths.push_back(g.length(*iterator));
        sum_edge_length += g.length(*iterator);
    }
    sort(lengths.begin(), lengths.end());
    double len_perc = (1.0 - percent * 0.01) * (double) (sum_edge_length);
    for (size_t i = 0; i < lengths.size(); i++) {
        if (lengths[i] >= len_perc)
            return lengths[i];
        else
            len_perc -= (double) lengths[i];
    }
    return 0;
}

}
