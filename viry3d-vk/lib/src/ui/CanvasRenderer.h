/*
* Viry3D
* Copyright 2014-2018 by Stack - stackos@qq.com
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "graphics/Renderer.h"
#include "container/Vector.h"
#include "container/Map.h"
#include "View.h"

namespace Viry3D
{
	class View;
	class Mesh;
    struct Touch;

    struct AtlasTreeNode
    {
        int x;
        int y;
        int w;
        int h;
        int layer;
        Vector<AtlasTreeNode*> children;
    };

	class CanvasRenderer : public Renderer
	{
	public:
		CanvasRenderer();
		virtual ~CanvasRenderer();
		virtual Ref<BufferObject> GetVertexBuffer() const;
		virtual Ref<BufferObject> GetIndexBuffer() const;
        virtual Ref<BufferObject> GetDrawBuffer() const { return m_draw_buffer; }
		virtual void Update();
        virtual void OnResize(int width, int height);
		void AddView(const Ref<View>& view);
		void RemoveView(const Ref<View>& view);
		void MarkCanvasDirty();

	private:
        void CreateMaterial();
        void NewAtlasTextureLayer();
        void UpdateProjectionMatrix();
        void UpdateCanvas();
        void UpdateAtlas(ViewMesh& mesh, bool& updated);
        AtlasTreeNode* FindAtlasTreeNodeToInsert(int w, int h, AtlasTreeNode* node);
        void ReleaseAtlasTreeNode(AtlasTreeNode* node);
        void HandleTouchEvent();
        void HitViews(const Touch& t);

	private:
		Vector<Ref<View>> m_views;
		bool m_canvas_dirty;
		Ref<Mesh> m_mesh;
        Ref<Texture> m_atlas;
        int m_atlas_array_size;
        Vector<AtlasTreeNode*> m_atlas_tree;
        Map<Texture*, AtlasTreeNode*> m_atlas_cache;
        Ref<BufferObject> m_draw_buffer;
        Vector<ViewMesh> m_view_meshes;
        Map<int, List<View*>> m_touch_down_views;
	};
}