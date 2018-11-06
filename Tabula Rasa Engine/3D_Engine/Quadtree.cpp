#include "Quadtree.h"
#include "trDefs.h"

#include "GameObject.h"

#define BUCKET_SIZE 2

Quadtree::Quadtree()
{
}

Quadtree::~Quadtree()
{
	RELEASE(root_node);
}

void Quadtree::Create(AABB limits)
{
	root_node = new QuadtreeNode(limits);
}

void Quadtree::Insert(GameObject * go)
{
	if (root_node != nullptr)
	{
		if (go->bounding_box.Intersects(root_node->box))
			root_node->Insert(go);
	}
}

void Quadtree::FillWithAABBs(std::vector<AABB>& vector)
{
	QuadtreeNode * node = root_node;

	IterateToFillAABBs(node, vector);
}

void Quadtree::IterateToFillAABBs(QuadtreeNode * node, std::vector<AABB>& vector)
{
	vector.push_back(node->box);

	if (node->childs[0] != nullptr) {
		for (uint i = 0u; i < 4; i++)
			IterateToFillAABBs(node->childs[i], vector);
	}
}

void Quadtree::CollectsGOs(const Frustum & frustum, std::vector<GameObject*>& go_output) const
{
	// Check it with root, then iterate
	root_node->CollectsGOs(frustum, go_output);
}

void Quadtree::Clear()
{
	RELEASE(root_node);
}

// ------------------------------------- NODE ---------------------------------------------------- \\

QuadtreeNode::QuadtreeNode(AABB limit)
{
	box = limit;
	for (uint i = 0; i < 4; i++)
		childs[i] = nullptr;
}

QuadtreeNode::~QuadtreeNode()
{
	if (childs[0] != nullptr) {
		for (uint i = 0u; i < 4; i++)
		{
			RELEASE(childs[i]);
		}
	}
}

void QuadtreeNode::Insert(GameObject * go)
{
	if (objects_inside.size() < BUCKET_SIZE) {
		objects_inside.push_back(go);
	}
	else {
		objects_inside.push_back(go);
		GenerateChilds();
		RedistributeObjects();
	}
}

bool QuadtreeNode::IsLeaf() const
{
	return (childs[0] == nullptr);
}

void QuadtreeNode::GenerateChilds()
{
	// Setting up
	float3 min_point = box.minPoint;
	float3 max_point = box.maxPoint;
	float width = math::Abs(min_point.x - max_point.x);
	float height = math::Abs(min_point.y - max_point.y);
	float depth = math::Abs(min_point.z - max_point.z);

	/*
		 __ __ __ __
		| CH1 | CH2	|
		|__ __|__ __|
		| CH3 |	CH4 |
		|__ __|__ __|

	*/

	// CH1
	float3 min_point_child = min_point;
	float3 max_point_child = min_point;
	max_point_child.x += width / 2.f;
	max_point_child.y += height;
	max_point_child.z += depth / 2.f;
	AABB child_box = AABB(min_point_child, max_point_child);
	childs[0] = new QuadtreeNode(child_box);

	// CH2
	min_point_child.x += width / 2.f;
	max_point_child.x += width / 2.f;
	child_box = AABB(min_point_child, max_point_child);
	childs[1] = new QuadtreeNode(child_box);

	// CH3
	min_point_child.x -= width / 2.f;
	max_point_child.x -= width / 2.f;
	min_point_child.z += depth / 2.f;
	max_point_child.z += depth / 2.f;
	child_box = AABB(min_point_child, max_point_child);
	childs[2] = new QuadtreeNode(child_box);

	// CH4
	min_point_child.x += width / 2.f;
	max_point_child.x += width / 2.f;
	child_box = AABB(min_point_child, max_point_child);
	childs[3] = new QuadtreeNode(child_box);
}

void QuadtreeNode::RedistributeObjects()
{
	for (std::list<GameObject*>::iterator it = objects_inside.begin(); it != objects_inside.end(); it++) {
		for (uint j = 0u; j < 4; j++)
		{
			if ((*it)->bounding_box.Intersects(childs[j]->box))
				childs[j]->Insert((*it));
		}
	}
}

void QuadtreeNode::CollectsGOs(const Frustum & frustum, std::vector<GameObject*>& go_output) const
{
	//check if the node's box intersects with the frustum
	if (FrustumContainsAaBox(this->box, frustum)) {

		for (std::list<GameObject*>::const_iterator it = objects_inside.begin(); it != objects_inside.end(); it++) {

			AABB go_box = (*it)->bounding_box;
			if (FrustumContainsAaBox(this->box, frustum)) { //if one game object of the node is in the frustum, collect it

				bool unique = true;
				for (uint i = 0; i < go_output.size(); i++)
				{
					if ((*it) == go_output.at(i))
						unique = false;
				}

				if(unique) // care iterating childs with the same gos
					go_output.push_back((*it));
			}

		}

		if (!IsLeaf()) {
			for (uint i = 0u; i < 4; i++)
			{
				childs[i]->CollectsGOs(frustum, go_output);
			}
		}

	}
}

bool QuadtreeNode::FrustumContainsAaBox(const AABB & ref_box, const Frustum & frustum) const
{
	float3 aabb_corners[8];
	int total_in = 0;
	ref_box.GetCornerPoints(aabb_corners);

	for (int p = 0; p < 6; ++p) { // Planes of the frustum
		int corners_outside = 8;
		int point_in = 1;

		for (int i = 0; i < 8; ++i) { // each corner of the AABB

			if (frustum.GetPlane(p).IsOnPositiveSide(aabb_corners[i])) {
				point_in = 0;
				--corners_outside;
			}
		}

		if (corners_outside == 0) // totally in
			return false;

		total_in += point_in;
	}
	//if (iTotalIn == 6)// TODO: maybe is usefull ...
	//return true;

	return true;
}