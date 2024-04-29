// Copyright (c) 2023 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

#include <filesystem>

#include <gtest/gtest.h>

#include "Application.h" // To set up a slice with settings.
#include "Slice.h" // To set up a scene to slice.
#include "geometry/OpenPolyline.h"
#include "geometry/Polygon.h" // Creating polygons to compare to sliced layers.
#include "slicer.h" // Starts the slicing phase that we want to test.
#include "utils/Coord_t.h"
#include "utils/Matrix4x3D.h" // To load STL files.
#include "utils/polygonUtils.h" // Comparing similarity of polygons.

namespace cura
{

class AdaptiveLayer;

/*
 * Integration test on the slicing phase of CuraEngine. This tests if the
 * slicing algorithm correctly splits a 3D model up into 2D layers.
 */
class SlicePhaseTest : public testing::Test
{
    void SetUp() override
    {
        // Start the thread pool
        Application::getInstance().startThreadPool();

        // Set up a scene so that we may request settings.
        Application::getInstance().current_slice_ = new Slice(1);

        // And a few settings that we want to default.
        Scene& scene = Application::getInstance().current_slice_->scene;
        scene.settings.add("slicing_tolerance", "middle");
        scene.settings.add("layer_height_0", "0.2");
        scene.settings.add("layer_height", "0.1");
        scene.settings.add("layer_0_z_overlap", "0.0");
        scene.settings.add("raft_airgap", "0.0");
        scene.settings.add("raft_base_thickness", "0.2");
        scene.settings.add("raft_interface_thickness", "0.2");
        scene.settings.add("raft_interface_layers", "1");
        scene.settings.add("raft_surface_thickness", "0.2");
        scene.settings.add("raft_surface_layers", "1");
        scene.settings.add("raft_surface_extruder_nr", "0");
        scene.settings.add("magic_mesh_surface_mode", "normal");
        scene.settings.add("meshfix_extensive_stitching", "false");
        scene.settings.add("meshfix_keep_open_polygons", "false");
        scene.settings.add("minimum_polygon_circumference", "1");
        scene.settings.add("meshfix_maximum_resolution", "0.04");
        scene.settings.add("meshfix_maximum_deviation", "0.02");
        scene.settings.add("meshfix_maximum_extrusion_area_deviation", "2000");
        scene.settings.add("wall_transition_angle", "10");
        scene.settings.add("xy_offset", "0");
        scene.settings.add("xy_offset_layer_0", "0");
        scene.settings.add("hole_xy_offset", "0");
        scene.settings.add("hole_xy_offset_max_diameter", "0");
        scene.settings.add("support_mesh", "false");
        scene.settings.add("anti_overhang_mesh", "false");
        scene.settings.add("cutting_mesh", "false");
        scene.settings.add("infill_mesh", "false");
        scene.settings.add("adhesion_type", "none");
    }
};

TEST_F(SlicePhaseTest, Cube)
{
    Scene& scene = Application::getInstance().current_slice_->scene;
    MeshGroup& mesh_group = scene.mesh_groups.back();

    const Matrix4x3D transformation;
    // Path to cube.stl is relative to CMAKE_CURRENT_SOURCE_DIR/tests.
    ASSERT_TRUE(loadMeshIntoMeshGroup(&mesh_group, std::filesystem::path(__FILE__).parent_path().append("resources/cube.stl").string().c_str(), transformation, scene.settings));
    EXPECT_EQ(mesh_group.meshes.size(), 1);
    Mesh& cube_mesh = mesh_group.meshes[0];

    const auto layer_thickness = scene.settings.get<coord_t>("layer_height");
    const auto initial_layer_thickness = scene.settings.get<coord_t>("layer_height_0");
    constexpr bool variable_layer_height = false;
    constexpr std::vector<AdaptiveLayer>* variable_layer_height_values = nullptr;
    const size_t num_layers = (cube_mesh.getAABB().max_.z_ - initial_layer_thickness) / layer_thickness + 1;
    Slicer slicer(&cube_mesh, layer_thickness, num_layers, variable_layer_height, variable_layer_height_values);

    ASSERT_EQ(slicer.layers.size(), num_layers) << "The number of layers in the output must equal the requested number of layers.";

    // Since a cube has the same slice at all heights, every layer must be the same square.
    Polygon square;
    square.emplace_back(0, 0);
    square.emplace_back(10000, 0); // 10mm cube.
    square.emplace_back(10000, 10000);
    square.emplace_back(0, 10000);

    for (size_t layer_nr = 0; layer_nr < num_layers; layer_nr++)
    {
        const SlicerLayer& layer = slicer.layers[layer_nr];
        EXPECT_EQ(layer.polygons.size(), 1);
        if (layer.polygons.size() == 1)
        {
            Polygon sliced_polygon = layer.polygons[0];
            EXPECT_EQ(sliced_polygon.size(), square.size());
            if (sliced_polygon.size() == square.size())
            {
                int start_corner = -1;
                for (size_t corner_idx = 0; corner_idx < square.size(); corner_idx++) // Find the starting corner in the sliced layer.
                {
                    if (square[corner_idx] == sliced_polygon[0])
                    {
                        start_corner = corner_idx;
                        break;
                    }
                }
                EXPECT_NE(start_corner, -1) << "The first vertex of the sliced polygon must be one of the vertices of the ground truth square.";

                if (start_corner != -1)
                {
                    for (size_t corner_idx = 0; corner_idx < square.size(); corner_idx++) // Check if every subsequent corner is correct.
                    {
                        EXPECT_EQ(square[(corner_idx + start_corner) % square.size()], sliced_polygon[corner_idx]);
                    }
                }
            }
        }
    }
}

TEST_F(SlicePhaseTest, Cylinder1000)
{
    Scene& scene = Application::getInstance().current_slice_->scene;
    MeshGroup& mesh_group = scene.mesh_groups.back();

    const Matrix4x3D transformation;
    // Path to cylinder1000.stl is relative to CMAKE_CURRENT_SOURCE_DIR/tests.
    ASSERT_TRUE(
        loadMeshIntoMeshGroup(&mesh_group, std::filesystem::path(__FILE__).parent_path().append("resources/cylinder1000.stl").string().c_str(), transformation, scene.settings));
    EXPECT_EQ(mesh_group.meshes.size(), 1);
    Mesh& cylinder_mesh = mesh_group.meshes[0];

    const auto layer_thickness = scene.settings.get<coord_t>("layer_height");
    const auto initial_layer_thickness = scene.settings.get<coord_t>("layer_height_0");
    constexpr bool variable_layer_height = false;
    constexpr std::vector<AdaptiveLayer>* variable_layer_height_values = nullptr;
    const size_t num_layers = (cylinder_mesh.getAABB().max_.z_ - initial_layer_thickness) / layer_thickness + 1;
    Slicer slicer(&cylinder_mesh, layer_thickness, num_layers, variable_layer_height, variable_layer_height_values);

    ASSERT_EQ(slicer.layers.size(), num_layers) << "The number of layers in the output must equal the requested number of layers.";

    // Since a cylinder has the same slice at all heights, every layer must be the same circle.
    constexpr size_t num_vertices = 1000; // Create a circle with this number of vertices (first vertex is in the +X direction).
    constexpr coord_t radius = 10000; // 10mm radius.
    Polygon circle;
    circle.reserve(num_vertices);
    for (size_t i = 0; i < 1000; i++)
    {
        const coord_t x = std::cos(std::numbers::pi * 2 / num_vertices * i) * radius;
        const coord_t y = std::sin(std::numbers::pi * 2 / num_vertices * i) * radius;
        circle.emplace_back(x, y);
    }
    Shape circles;
    circles.push_back(circle);

    for (size_t layer_nr = 0; layer_nr < num_layers; layer_nr++)
    {
        const SlicerLayer& layer = slicer.layers[layer_nr];
        EXPECT_EQ(layer.polygons.size(), 1);
        if (layer.polygons.size() == 1)
        {
            Polygon sliced_polygon = layer.polygons[0];
            // Due to the reduction in resolution, the final slice will not have the same vertices as the input.
            // Let's say that are allowed to be up to 1/500th of the surface area off.
            EXPECT_LE(PolygonUtils::relativeHammingDistance(layer.polygons, circles), 0.002);
        }
    }
}

} // namespace cura
