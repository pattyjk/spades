#pragma once

#include "drawing_commands/drawing_command.hpp"
#include "drawing_commands/draw_position_command.hpp"
#include "drawing_commands/show_position_command.hpp"
#include "drawing_commands/draw_part_of_genome_command.hpp"

#include "environment.hpp"
#include "command.hpp"
#include "command_type.hpp"
#include "errors.hpp"
#include "argument_list.hpp"

namespace online_visualization {

    class DrawVertexCommand : public DrawingCommand {
        protected:
            size_t MinArgNumber() const {
                return 1;   
            }
            
            bool CheckCorrectness(const vector<string>& args) const {
                if (!CheckEnoughArguments(args))
                    return false;
                bool result = true;
                result = result & CheckIsNumber(args[0]);
                return result;
            }

        public:
            string Usage() const {
                string answer;
                answer = answer + "Command `draw_vertex` \n" + 
                                "Usage:\n" + 
                                "> vertex <vertex_id>\n" + 
                                " This command prints pictures for a neigbourhood of a vertex in the DB graph.\n" + 
                                " You should specify an id of the vertex in the DB graph, which neighbourhood you want to look at.";
                return answer;
            }
            
            DrawVertexCommand() : DrawingCommand(CommandType::draw_vertex)
            {
            }

            void Execute(Environment& curr_env, const ArgumentList& args) const {
                const vector<string>& args_ = args.GetAllArguments();

                if (!CheckCorrectness(args_))
                    return;
                size_t vertex_id = GetInt(args_[0]);
                if (CheckVertexExists(curr_env.int_ids(), vertex_id)) 
                    DrawVertex(curr_env, vertex_id, args_[0]);
            }
    };

    class DrawEdgeCommand : public DrawingCommand {
        protected:
            size_t MinArgNumber() const {
                return 1;   
            }
            
            bool CheckCorrectness(const vector<string>& args) const {
                return CheckEnoughArguments(args);
            }

            void DrawEdge(Environment& curr_env, EdgeId edge, string label = "") const {
                DrawingCommand::DrawPicture(curr_env, curr_env.graph().EdgeStart(edge), label); 
            }

            void DrawEdge(Environment& curr_env, size_t edge_id, string label = "") const {
                DrawEdge(curr_env, curr_env.int_ids().ReturnEdgeId(edge_id), label);
            }

        public:
            string Usage() const {
                string answer;
                answer = answer + "Command `draw_edge` \n" + 
                                "Usage:\n" + 
                                "> edge <edge_id>\n" + 
                                " This command prints pictures for a neigbourhood of an edge in the DB graph.\n" + 
                                " You should specify an id of the edge in the DB graph, which location you want to look at.";
                return answer;
            }

            DrawEdgeCommand() : DrawingCommand(CommandType::draw_edge)
            {
            }

            void Execute(Environment& curr_env, const ArgumentList& arg_list) const {
                const vector<string>& args_ = arg_list.GetAllArguments();

                if (!CheckCorrectness(args_))
                     return;

                size_t edge_id = GetInt(args_[0]);
                if (CheckEdgeExists(curr_env.int_ids(), edge_id)) {
                    DrawEdge(curr_env, edge_id, args_[0]);
                }
            }
    };
}

