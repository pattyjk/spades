#include "cute.h"
#include <set>
#include "seq.hpp"
#include "sequence.hpp"
#include "nucl.hpp"
#include "debruijn.hpp"
#include "graphVisualizer.hpp"
#include <fstream>
#include <iostream>

using namespace std;

void TestAddNode() {
	DeBruijn<5> g;
	Seq<5, int> seq1("ACAAA");
	Seq<5, int> seq2("CAAAC");
	Seq<5, int> seq3("CAAAA");
	g.addNode(seq1);
	g.addNode(seq2);
	g.addNode(seq3);
	int c = 0;
	for (DeBruijn<5>::kmer_iterator it = g.kmer_begin(); it != g.kmer_end(); it++) {
		c++;
		ASSERT(*it == seq1 || *it == seq2 || *it == seq3);
	}
	ASSERT_EQUAL(3, c);
}

void TestAddEdge() {
	DeBruijn<5> g;
	Seq<5, int> seq1("ACAAA");
	Seq<5, int> seq2("CAAAG");
	Seq<5, int> seq3("CAAAA");
	g.addEdge(seq1, seq2);
	g.addEdge(seq1, seq3);
	int c = 0;
	for (DeBruijn<5>::kmer_iterator it = g.kmer_begin(); it != g.kmer_end(); it++) {
		c++;
		ASSERT(*it == seq1 || *it == seq2 || *it == seq3);
	}
	ASSERT_EQUAL(3, c);
	ASSERT_EQUAL(2, g.NextCount(seq1));
	DeBruijn<5>::neighbour_iterator n_it = g.begin_next(seq1);
	ASSERT_EQUAL(seq3, *n_it);
	++n_it;
	ASSERT_EQUAL(seq2, *n_it);
}

void TestAddEdge2() {
	DeBruijn<5> g;
	Seq<5, int> seq1("ACAAA");
	Seq<5, int> seq2("CCAAA");
	Seq<5, int> seq3("CAAAA");
	g.addEdge(seq1, seq3);
	g.addEdge(seq2, seq3);
	int c = 0;
	for (DeBruijn<5>::kmer_iterator it = g.kmer_begin(); it != g.kmer_end(); it++) {
		c++;
		ASSERT(*it == seq1 || *it == seq2 || *it == seq3);
	}
	ASSERT_EQUAL(3, c);
	ASSERT_EQUAL(2, g.PrevCount(seq3));
	DeBruijn<5>::neighbour_iterator n_it = g.begin_prev(seq3);
	ASSERT_EQUAL(seq1, *n_it);
	++n_it;
	ASSERT_EQUAL(seq2, *n_it);
}

void TestSimpleConstruction() {
	string ss[] = { "CGAAACCAC", "CGAAAACAC", "AACCACACC", "AAACACACC" };
	vector<strobe_read<9, 4> > input;
	input.push_back(strobe_read<9, 4> (ss));
	DeBruijn<5> g;
	g.ConstructGraph(input);
	int c = 0;
	Seq<5> seq("CGAAA");
	Seq<5> seq2("CACAC");
	for (DeBruijn<5>::kmer_iterator it = g.kmer_begin(); it != g.kmer_end(); it++) {
		c++;
	}
	ASSERT_EQUAL(26, c);
	ASSERT_EQUAL(2, g.NextCount(seq));
	DeBruijn<5>::neighbour_iterator n_it = g.begin_next(seq);
	ASSERT_EQUAL(Seq<5>("GAAAA"), *n_it);
	++n_it;
	ASSERT_EQUAL(Seq<5>("GAAAC"), *n_it);

	ASSERT_EQUAL(2, g.PrevCount(seq2));
	DeBruijn<5>::neighbour_iterator n_it2 = g.begin_prev(seq2);
	ASSERT(Seq<5>("ACACA") == *n_it2);
	++n_it2;
	ASSERT_EQUAL(Seq<5>("CCACA"), *n_it2);
}

cute::suite DeBruijnGraphSuite() {
	cute::suite s;
	s.push_back(CUTE(TestAddNode));
	s.push_back(CUTE(TestAddEdge));
	s.push_back(CUTE(TestAddEdge2));
	s.push_back(CUTE(TestSimpleConstruction));
	return s;
}
