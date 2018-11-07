#include "MeshImporter.h"

#include "trDefs.h"
#include "trLog.h"
#include "trApp.h"
#include "trRenderer3D.h"
#include "trTextures.h"
#include "trCamera3D.h"
#include "trMainScene.h"
#include "trEditor.h"
#include "trFileSystem.h"

#include "GameObject.h"
#include "ComponentMesh.h"
#include "ComponentMaterial.h"
#include "ComponentTransform.h"
#include "ComponentCamera.h"

#include "MathGeoLib/MathGeoLib.h"

#include "Assimp/include/cimport.h"
#include "Assimp/include/scene.h"
#include "Assimp/include/postprocess.h"
#include "Assimp/include/cfileio.h"

#pragma comment (lib, "Assimp/libx86/assimp-vc140-mt.lib")

void StreamCallback(const char* msg, char* user_msg) {
	TR_LOG("trFileLoader: %s", msg);
}

MeshImporter::MeshImporter()
{
	TR_LOG("MeshImporter: Loading Mesh Importer");
	bool ret = true;

	// Stream log messages to Debug window
	struct aiLogStream stream;
	stream.callback = StreamCallback;
	aiAttachLogStream(&stream);
	stream = aiGetPredefinedLogStream(aiDefaultLogStream_DEBUGGER, nullptr);
}

MeshImporter::~MeshImporter()
{
	// Clean all log streams
	aiDetachAllLogStreams();
}

bool MeshImporter::Import(const char * path, std::string & output_file)
{
	TR_LOG("trFileLoader: Start importing a file with path: %s", path);

	//TODO CHECK PATH
	const aiScene* scene = aiImportFile(path, aiProcessPreset_TargetRealtime_MaxQuality);

	if (scene != nullptr) {
		App->main_scene->ClearScene();
		// Removing and cleaning the last AABB
		scene_num_vertex = 0u;
		scene_vertices.clear();
		cursor_data = nullptr;
		material_data = nullptr;
		ImportNodesRecursively(scene->mRootNode, scene, App->main_scene->GetRoot(), (char*)path);

		// Camera AABB stuff
		if (scene->mRootNode->mNumChildren == 1) { // if only one mesh, get the bounding_box of the last mesh
			App->editor->SetSelected(model_root);
			App->camera->dummy_camera->FocusOnAABB(App->camera->dummy_camera->last_aabb);
		}
		else { // get the bouncing of all the meshes
			model_bouncing_box = AABB(float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f));
			model_bouncing_box.Enclose((float3*)&scene_vertices.front(), scene_num_vertex);
			//	model_root->bounding_box = model_bouncing_box;
			//	App->editor->SetSelected(model_root);
			// TODO: calculate it better
			App->camera->dummy_camera->FocusOnAABB(model_bouncing_box);
		}

		App->main_scene->GetRoot()->RecalculateBoundingBox();

		for (std::list<GameObject*>::iterator it = App->main_scene->GetRoot()->childs.begin(); it != App->main_scene->GetRoot()->childs.end(); it++)
			(*it)->is_static = true;

		for (std::list<GameObject*>::iterator it = App->main_scene->GetRoot()->childs.begin(); it != App->main_scene->GetRoot()->childs.end(); it++) {
			if ((*it)->is_static) {
				App->main_scene->InsertGoInQuadtree((*it));
			}
		}

		aiReleaseImport(scene);

		return true;

	}

	TR_LOG("trFileLoader: Error importing a file: %s", path);

	aiReleaseImport(scene);

	return false;
}

bool MeshImporter::Import(const void * buffer, uint size, std::string & output_file)
{
	return false;
}



void MeshImporter::ImportNodesRecursively(const aiNode * node, const aiScene * scene, GameObject * parent_go, char* file_path)
{

	GameObject* new_go = nullptr;

	if (node->mNumMeshes > 0) //if this node have a mesh
	{
		new_go = App->main_scene->CreateGameObject(node->mName.C_Str(), parent_go);
		std::string tmp = "";
		if (file_path != nullptr) {
			tmp = file_path;
			// Let's get the file name to print it in inspector:
			const size_t last_slash = tmp.find_last_of("\\/");
			if (std::string::npos != last_slash)
				tmp.erase(0, last_slash + 1);
			const size_t extension = tmp.rfind('.');
			if (std::string::npos != extension)
				tmp.erase(extension);
			file_name = tmp;
			//new_go->SetName(tmp.c_str());
			model_root = new_go;
			file_path = nullptr;
		}

		// Calculate the position, scale and rotation
		aiVector3D translation;
		aiVector3D scaling;
		aiQuaternion rotation;
		node->mTransformation.Decompose(scaling, rotation, translation);
		Quat rot(rotation.x, rotation.y, rotation.z, rotation.w);
		float3 quat_to_euler = rot.ToEulerXYZ(); // transforming it to euler to show it in inspector

		ComponentTransform* transform_comp = (ComponentTransform*)new_go->CreateComponent(Component::component_type::COMPONENT_TRANSFORM);
		transform_comp->Setup(float3(translation.x, translation.y, translation.z), float3(scaling.x, scaling.y, scaling.z), rot);

		mesh_data = new Mesh(); // our mesh

		aiMesh* new_mesh = scene->mMeshes[node->mMeshes[0]];

		// Getting texture material if needed	
		if (scene->mMaterials[new_mesh->mMaterialIndex] != nullptr) {
			if (material_data == nullptr)
				material_data = LoadTexture(scene->mMaterials[new_mesh->mMaterialIndex], new_go);
			else
				material_data = (ComponentMaterial*)new_go->CreateComponent(Component::component_type::COMPONENT_MATERIAL, material_data);
		}

		// Vertex copy
		mesh_data->vertex_size = new_mesh->mNumVertices;
		mesh_data->vertices = new float[mesh_data->vertex_size * 3];
		memcpy(mesh_data->vertices, new_mesh->mVertices, sizeof(float) * mesh_data->vertex_size * 3);

		// Data for the bounding box of all the meshes
		for (uint i = 0; i < mesh_data->vertex_size; i++) {
			scene_vertices.push_back(float3(mesh_data->vertices[i], mesh_data->vertices[i + 1], mesh_data->vertices[i + 2]));
		}
		scene_num_vertex += mesh_data->vertex_size;

		// UVs copy
		if (new_mesh->HasTextureCoords(0)) {//i?
			mesh_data->size_uv = new_mesh->mNumVertices;
			mesh_data->uvs = new float[mesh_data->size_uv * 2];
			for (int i = 0; i < mesh_data->size_uv; i++) {
				mesh_data->uvs[i * 2] = new_mesh->mTextureCoords[0][i].x;
				mesh_data->uvs[i * 2 + 1] = new_mesh->mTextureCoords[0][i].y;
			}
		}
		else {
			mesh_data->size_uv = 0;
			mesh_data->uvs = nullptr;
		}

		// Index copy
		if (new_mesh->HasFaces())
		{
			mesh_data->face_size = new_mesh->mNumFaces;
			mesh_data->index_size = new_mesh->mNumFaces * 3;
			mesh_data->indices = new uint[mesh_data->index_size]; // assume each face is a triangle
			for (uint i = 0; i < new_mesh->mNumFaces; ++i)
			{
				if (new_mesh->mFaces[i].mNumIndices != 3) {
					TR_LOG("WARNING, geometry face with != 3 indices!");
				}
				else
					memcpy(&mesh_data->indices[i * 3], new_mesh->mFaces[i].mIndices, 3 * sizeof(uint));
			}
		}

		// Generating bounding box
		new_go->bounding_box = AABB(float3(0.f, 0.f, 0.f), float3(0.f, 0.f, 0.f));
		new_go->bounding_box.Enclose((float3*)mesh_data->vertices, mesh_data->vertex_size);

		ComponentMesh* mesh_comp = (ComponentMesh*)new_go->CreateComponent(Component::component_type::COMPONENT_MESH);
		mesh_comp->SetMesh(mesh_data);

		//testing zone
		// check if there is already a file
		// todo finish this
		SaveMeshFile(new_go->GetName(), mesh_data);

	}
	for (uint i = 0; i < node->mNumChildren; i++)
		ImportNodesRecursively(node->mChildren[i], scene, new_go, file_path);


}


ComponentMaterial * MeshImporter::LoadTexture(aiMaterial* material, GameObject* go)
{
	// Getting the texture path
	aiString tmp_path;
	std::string texture_path;
	material->GetTexture(aiTextureType_DIFFUSE, 0, &tmp_path);
	texture_path = tmp_path.data;

	// Let's get ONLY the file name:
	const size_t last_slash = texture_path.find_last_of("\\/");
	if (std::string::npos != last_slash)
		texture_path.erase(0, last_slash + 1);

	// Let's search the texture in our path assets/textures
	if (!texture_path.empty()) {
		std::string posible_path = "assets/textures/";
		posible_path = posible_path + texture_path;
		TR_LOG("trFileLoader: Search in - %s", posible_path.c_str());
		ComponentMaterial* material_comp = (ComponentMaterial*)go->CreateComponent(Component::component_type::COMPONENT_MATERIAL);
		material_comp->SetTexture(App->texture->LoadImageFromPath(posible_path.c_str()));

		// Material color of the mesh
		aiColor4D tmp_color;
		aiGetMaterialColor(material, AI_MATKEY_COLOR_AMBIENT, &tmp_color);
		material_comp->SetAmbientColor(float4(tmp_color.r, tmp_color.g, tmp_color.b, tmp_color.a));

		return material_comp;
	}
	else {
		TR_LOG("trFileLoader: Didn't find any embeded texture");
		return nullptr;
	}

}


bool MeshImporter::SaveMeshFile(const char* file_name, Mesh* mesh_data)
{
	uint size_indices = sizeof(uint) * mesh_data->index_size;
	uint size_vertices = sizeof(float) * (mesh_data->vertex_size * 3);
	uint size_uvs = sizeof(float) * (mesh_data->size_uv * 2);

	// amount of indices / vertices / colors / normals / texture_coords / AABB
	uint ranges[3] = { mesh_data->index_size, mesh_data->vertex_size, mesh_data->size_uv };
	uint size = sizeof(ranges) + size_indices + size_vertices + size_uvs;

	char* data = new char[size]; // Allocate
	char* cursor = data;

	uint bytes = sizeof(ranges); // First store ranges
	memcpy(cursor, ranges, bytes);

	cursor += bytes;
	bytes = size_indices; // Store indices
	memcpy(cursor, mesh_data->indices, bytes);

	cursor += bytes;
	bytes = size_vertices; // Store vertices
	memcpy(cursor, mesh_data->vertices, bytes);

	cursor += bytes;
	bytes = size_uvs; // Store uvs
	memcpy(cursor, mesh_data->uvs, bytes);

	// Saving file

	std::string tmp_str(MESH_DIR);
	tmp_str.append("/");
	tmp_str.append(file_name);
	tmp_str.append(".tr"); // adding our own format extension

	App->file_system->WriteInFile(tmp_str.c_str(), cursor, size);

	// deleting useless data
	RELEASE_ARRAY(data);

	return true;
}