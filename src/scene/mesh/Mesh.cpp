#include <cmath>
#include <cstring>

#include "Mesh.h"

using namespace std;

namespace scene {
namespace mesh {

Mesh::Mesh( const size_t vertex_count, const size_t surface_count )
	: m_vertex_count( vertex_count )
	, m_surface_count( surface_count )
	, m_index_count( surface_count * SURFACE_SIZE )
{
	m_vertex_data = (uint8_t*)malloc( GetVertexDataSize() );
	m_index_data = (uint8_t*)malloc( GetIndexDataSize() );
}

Mesh::~Mesh() {
	free( m_vertex_data );
	free( m_index_data );
}

void Mesh::Clear() {
	memset( ptr( m_vertex_data, 0, GetVertexDataSize() ), 0, GetVertexDataSize() );
	memset( ptr( m_index_data, 0, GetIndexDataSize() ), 0, GetIndexDataSize() );
	m_is_final = true;
}

void Mesh::AddSurface( const Mesh::surface_t& surface  ) {
	ASSERT( !m_is_final, "addsurface on already finalized mesh" );
	ASSERT( m_surface_i < m_surface_count, "surface out of bounds" );
	// add triangle
	memcpy( ptr( m_index_data, m_surface_i * SURFACE_SIZE * sizeof( index_t ), sizeof( surface ) ), &surface, sizeof( surface ) );
	m_surface_i++;
}

Mesh::index_t Mesh::AddVertex( const Vec3 &coord, const Vec2<Mesh::coord_t> &tex_coord ) {
	ASSERT( !m_is_final, "addvertex on already finalized mesh" );
	ASSERT( m_vertex_i < m_vertex_count, "vertex out of bounds (" + to_string( m_vertex_i ) + " >= " + to_string( m_vertex_count ) + ")" );
	memcpy( ptr( m_vertex_data, m_vertex_i * VERTEX_SIZE * sizeof( coord_t ), sizeof( coord ) ), &coord, sizeof(coord) );
	memcpy( ptr( m_vertex_data, m_vertex_i * VERTEX_SIZE * sizeof( coord_t ) + VERTEX_COORD_SIZE * sizeof( coord_t ), sizeof( tex_coord ) ), &tex_coord, sizeof(tex_coord) );
	Mesh::index_t ret = m_vertex_i;
	m_vertex_i++;
	return ret;
}
Mesh::index_t Mesh::AddVertex( const Vec2<Mesh::coord_t> &coord, const Vec2<Mesh::coord_t> &tex_coord ) {
	return AddVertex( Vec3( coord.x, coord.y, 0.0 ), tex_coord );
}

void Mesh::SetVertex( const index_t index, const Vec3 &coord, const Vec2<Mesh::coord_t> &tex_coord ) {
	ASSERT( index < m_vertex_count, "index out of bounds" );
	memcpy( ptr( m_vertex_data, index * VERTEX_SIZE * sizeof( coord_t ), sizeof( coord ) ), &coord, sizeof( coord ) );
	memcpy( ptr( m_vertex_data, index * VERTEX_SIZE * sizeof( coord_t ) + VERTEX_COORD_SIZE * sizeof( coord_t ), sizeof( tex_coord ) ), &tex_coord, sizeof( tex_coord ) );
	Update();
}
void Mesh::SetVertex( const index_t index, const Vec2<Mesh::coord_t> &coord, const Vec2<Mesh::coord_t> &tex_coord ) {
	SetVertex( index, { coord.x, coord.y, 0.0 }, tex_coord );
}

void Mesh::SetVertexCoord( const index_t index, const Vec3 &coord ) {
	ASSERT( index < m_vertex_count, "index out of bounds" );
	memcpy( ptr( m_vertex_data, index * VERTEX_SIZE * sizeof( coord_t ), sizeof( coord ) ), &coord, sizeof( coord ) );
	Update();
}
void Mesh::SetVertexCoord( const index_t index, const Vec2<Mesh::coord_t> &coord ) {
	SetVertexCoord( index, { coord.x, coord.y, 0.0 } );
}

void Mesh::SetVertexTexCoord( const index_t index, const Vec2<Mesh::coord_t> &tex_coord ) {
	ASSERT( index < m_vertex_count, "index out of bounds" );
	memcpy( ptr( m_vertex_data, index * VERTEX_SIZE * sizeof( coord_t ) + VERTEX_COORD_SIZE * sizeof( coord_t ), sizeof( tex_coord ) ), &tex_coord, sizeof( tex_coord ) );
	Update();
}

void Mesh::SetSurface( const index_t index, const Mesh::surface_t& surface ) {
	ASSERT( index < m_surface_count, "surface out of bounds" );
	// add triangle
	memcpy( ptr( m_index_data, index * SURFACE_SIZE * sizeof( index_t ), sizeof( surface ) ), &surface, sizeof( surface ) );
}


void Mesh::Finalize() {
	ASSERT( !m_is_final, "finalize on already finalized mesh" );
	ASSERT( m_vertex_i == m_vertex_count, "vertex data not fully initialized on finalize" );
	ASSERT( m_surface_i == m_surface_count, "surface data not fully initialized on finalize" );
	m_is_final = true;
	Update();
}

void Mesh::GetVertexCoord( const index_t index, Vec3* coord ) {
	ASSERT( index < m_vertex_count, "index out of bounds" );
	memcpy( coord, ptr( m_vertex_data, index * VERTEX_SIZE * sizeof( coord_t ), sizeof( coord ) ), sizeof( coord ) );
}

const size_t Mesh::GetVertexCount() const {
	return m_vertex_count;
}
const size_t Mesh::GetVertexDataSize() const {
	return m_vertex_count * VERTEX_SIZE * sizeof( coord_t );
}
const uint8_t* Mesh::GetVertexData() const {
	return m_vertex_data;
}

const size_t Mesh::GetIndexCount() const {
	return m_index_count;
}
const size_t Mesh::GetSurfaceCount() const {
	return m_surface_count;
}
const size_t Mesh::GetIndexDataSize() const {
	return m_index_count * sizeof( index_t );
}
const uint8_t* Mesh::GetIndexData() const {
	return m_index_data;
}

void Mesh::Update() {
	m_update_counter++;
}

const size_t Mesh::UpdatedCount() const {
	return m_update_counter;
}

} /* namespace mesh */
} /* namespace scene */
