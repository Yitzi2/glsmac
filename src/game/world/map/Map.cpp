#include <regex>

#include "Map.h"

#include "engine/Engine.h"

#include "types/mesh/Render.h"
#include "types/mesh/Data.h"
#include "scene/actor/Instanced.h"
#include "scene/actor/Mesh.h"

#include "module/Prepare.h"
#include "module/LandMoisture.h"
#include "module/LandSurface.h"
#include "module/LandSurfacePP.h"
#include "module/WaterSurface.h"
#include "module/WaterSurfacePP.h"
#include "module/CalculateCoords.h"
#include "module/Coastlines1.h"
#include "module/Coastlines2.h"
#include "module/Sprites.h"
#include "module/Minimap.h"
#include "module/Finalize.h"

#include "scene/actor/Sprite.h"

using namespace scene::actor;

namespace game {
namespace world {
namespace map {

const Map::consts_t Map::s_consts = {};

#define B(x) S_to_binary_(#x)

static inline unsigned char S_to_binary_(const char *s)
{
    unsigned long long i = 0;
    while (*s) {
        i <<= 1;
        i += *s++ - '0';
    }
    return i;
}

Map::Map( Random* random, Scene* scene )
	: m_random( random )
	, m_scene( scene )
{

	// add texture variant bitmap maps
	CalculateTextureVariants( TVT_TILES, {
		{ B(00000000), B(10101010) }, // 0
		{ B(00001000), B(10101010) }, // 1
		{ B(00101000), B(10111010) }, // 2
		{ B(00111000), B(10111010) }, // 3
		{ B(10001000), B(10101010) }, // 4
		{ B(10101000), B(11111010) }, // 5
		{ B(10111000), B(11111010) }, // 6
		{ B(11101000), B(11111010) }, // 7
		{ B(11111000), B(11111010) }, // 8
		{ B(10101010), B(11111111) }, // 9
		{ B(10101110), B(11111111) }, // 10
		{ B(10111110), B(11111111) }, // 11
		{ B(11101110), B(11111111) }, // 12
		{ B(11111110), B(11111111) }, // 13
		{ B(11111111), B(11111111) }, // 14
		// 15 ???
	});
	CalculateTextureVariants( TVT_RIVERS, {
		{ B(00000000), B(10101010) }, // 0
		{ B(00100000), B(10101010) }, // 1
		{ B(10000000), B(10101010) }, // 2
		{ B(10100000), B(10101010) }, // 3
		{ B(00000010), B(10101010) }, // 4
		{ B(00100010), B(10101010) }, // 5
		{ B(10000010), B(10101010) }, // 6
		{ B(10100010), B(10101010) }, // 7
		{ B(00001000), B(10101010) }, // 8
		{ B(00101000), B(10101010) }, // 9
		{ B(10001000), B(10101010) }, // 10
		{ B(10101000), B(10101010) }, // 11
		{ B(00001010), B(10101010) }, // 12
		{ B(00101010), B(10101010) }, // 13
		{ B(10001010), B(10101010) }, // 14
		{ B(10101010), B(10101010) }, // 15
	});

	// main source textures
	m_textures.source.texture_pcx = g_engine->GetTextureLoader()->LoadTextureTC( "texture.pcx", Color::RGBA( 125, 0, 128, 255 ) );
	m_textures.source.ter1_pcx = g_engine->GetTextureLoader()->LoadTextureTCs( "ter1.pcx", {
		Color::RGBA( 152, 24, 228, 255 ), // remove transparency color
		Color::RGBA( 253, 189, 118, 255 ) // remove drawn shadows too (we'll have our own)
	});
	
	// add map modules
	//   order of passes is important
	//   order of modules within pass is important too
	
	Module* m;
	module_pass_t module_pass;
	
	{ // prepare tile states
		module_pass.clear();
		NEW( m, Prepare, this ); module_pass.push_back( m ); // needs to always be first
		m_modules.push_back( module_pass );
	}
	
	{ // needs to be in separate pass because moisture original textures need to be available for all tiles in next pass
		module_pass.clear();
		NEW( m, LandMoisture, this ); module_pass.push_back( m );
		m_modules.push_back( module_pass );
	}
	
	{ // main pass
		module_pass.clear();
		NEW( m, LandSurface, this ); module_pass.push_back( m );
		NEW( m, WaterSurface, this ); module_pass.push_back( m );
		NEW( m, CalculateCoords, this ); module_pass.push_back( m );
		NEW( m, Coastlines1, this ); module_pass.push_back( m );
		//NEW( m, Minimap, this ); module_pass.push_back( m ); TODO: remake via fbo
		m_modules.push_back( module_pass );
	}
	
	{ // second pass. needed to read and write tile states of tile or neighbors after they are fully generated in main pass
		module_pass.clear();
		NEW( m, Coastlines2, this ); module_pass.push_back( m );
		m_modules.push_back( module_pass );
	}
	
	{ // finalize and write vertices to mesh
		module_pass.clear();
		NEW( m, Finalize, this ); module_pass.push_back( m );
		m_modules.push_back( module_pass );
	}
	
	// deferred passes (after all other passes and deferred calls are processed)
	{
		module_pass.clear();
		NEW( m, LandSurfacePP, this ); module_pass.push_back( m );
		m_modules_deferred.push_back( module_pass );
	}
	{
		module_pass.clear();
		NEW( m, WaterSurfacePP, this ); module_pass.push_back( m );
		NEW( m, Sprites, this ); module_pass.push_back( m );
		m_modules_deferred.push_back( module_pass );
	}
}

Map::~Map() {
	if ( m_tile_at_request_id ) {
		m_actors.terrain->GetMeshActor()->CancelDataRequest( m_tile_at_request_id );
	}
	if ( m_actors.terrain ) {
		m_scene->RemoveActor( m_actors.terrain );
		DELETE( m_actors.terrain );
	}
	if ( m_tiles ) {
		DELETE( m_tiles );
	}
	if ( m_textures.terrain ) {
		DELETE( m_textures.terrain );
	}
	if ( m_textures.minimap ) {
		DELETE( m_textures.minimap );
	}
	for ( auto& module_pass : m_modules ) {
		for ( auto& m : module_pass ) {
			DELETE( m );
		}
	}
	for ( auto& module_pass : m_modules_deferred ) {
		for ( auto& m : module_pass ) {
			DELETE( m );
		}
	}
	if ( m_tile_states ) {
		for ( size_t y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
			for ( size_t x = y & 1 ; x < m_map_state.dimensions.x ; x+= 2 ) {
				
				auto* ts = GetTileState( x, y );
				
				if ( ts->moisture_original ) {
					DELETE( ts->moisture_original );
					ts->moisture_original = nullptr;
				}
				if ( ts->river_original ) {
					DELETE( ts->river_original );
					ts->river_original = nullptr;
				}
				for ( auto& a : ts->sprites ) {
					m_scene->RemoveActor( a.actor );
					DELETE( a.actor );
				}
			}
		}
		free( m_tile_states );
	}
}

void Map::SetTiles( Tiles* tiles, bool generate_actors ) {
	ASSERT( !m_tiles, "map tiles already set" );
	m_tiles = tiles;
	if ( generate_actors ) {
		GenerateActors();
	}
}

#ifdef DEBUG
std::vector<actor::Instanced*> Map::GetActors() const {
	return {
		m_actors.terrain,
	};
}
#endif

void Map::LinkTileStates() {
	
	ASSERT( !m_tile_states, "m_tile_states already set" );
	{
		size_t sz = sizeof( tile_state_t ) * m_map_state.dimensions.y * m_map_state.dimensions.x;
		m_tile_states = (tile_state_t*)malloc( sz );
		memset( ptr( m_tile_states, 0, sz ), 0, sz );
	}

	Log( "Linking tile states" );
	
	// link to each other via pointers
	// TODO: refactor this and Tiles
	for ( auto y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
		for ( auto x = y & 1 ; x < m_map_state.dimensions.x ; x += 2 ) {
			auto* ts = GetTileState( x, y );
			
			ts->W = ( x >= 2 ) ? GetTileState( x - 2, y ) : GetTileState( m_map_state.dimensions.x - 1 - ( 1 - ( y % 2 ) ), y );
			ts->NW = ( y >= 1 )
				? ( ( x >= 1 )
					? GetTileState( x - 1, y - 1 )
					: GetTileState( m_map_state.dimensions.x - 1, y - 1 )
				)
				: ts
			;
			ts->N = ( y >= 2 ) ? GetTileState( x, y - 2 ) : ts;
			ts->NE = ( y >= 1 )
				? ( ( x < m_map_state.dimensions.x - 1 )
					? GetTileState( x + 1, y - 1 )
					: GetTileState( 0, y - 1 )
				)
				: ts
			;
			ts->E = ( x < m_map_state.dimensions.x - 2 ) ? GetTileState( x + 2, y ) : GetTileState( y % 2, y );
			ts->SE = ( y < m_map_state.dimensions.y - 1 )
				? ( ( x < m_map_state.dimensions.x - 1 )
					? GetTileState( x + 1, y + 1 )
					: GetTileState( 0, y + 1 )
				)
				: ts
			;
			ts->S = ( y < m_map_state.dimensions.y - 2 ) ? GetTileState( x, y + 2 ) : ts;
			ts->SW = ( y < m_map_state.dimensions.y - 1 )
				? ( ( x >= 1 )
					? GetTileState( x - 1, y + 1 )
					: GetTileState( m_map_state.dimensions.x - 1, y + 1 )
				)
				: ts
			;

		}
	}
}

void Map::RunModulePasses( module_passes_t& module_passes ) {
	for ( auto& module_pass : module_passes ) {
		for ( size_t y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
			for ( size_t x = y & 1 ; x < m_map_state.dimensions.x ; x+= 2 ) {
				
				m_current_tile = m_tiles->At( x, y );
				m_current_ts = GetTileState( x, y );

				for ( auto& m : module_pass ) {
					m->GenerateTile( m_current_tile, m_current_ts, &m_map_state );
				}
			}
		}
	}
}

void Map::InitTextureAndMesh() {
	
	if ( m_textures.terrain ) {
		DELETE( m_textures.terrain );
	}
	NEW( m_textures.terrain, Texture, "TerrainTexture",
		( m_map_state.dimensions.x + 1 ) * Map::s_consts.tile_texture.dimensions.x, // + 1 for overdraw_column
		( m_map_state.dimensions.y * LAYER_MAX ) * Map::s_consts.tile_texture.dimensions.y
	);
	
	if ( m_textures.minimap ) {
		DELETE( m_textures.minimap );
	}
	// packed coordinates
	// 1px per tile (TODO: make higher resolution?)	
	NEW( m_textures.minimap, Texture, "MinimapTexture",
		( m_map_state.dimensions.x / 2 ) + 1,
		m_map_state.dimensions.y / 2 
	);
	
	// not deleting meshes because if they exist - it means they are already linked to actor and are deleted together when needed
	NEW( m_mesh_terrain, types::mesh::Render,
		( m_map_state.dimensions.x * LAYER_MAX + 1 ) * m_map_state.dimensions.y * 5 / 2, // + 1 for overdraw column
		( m_map_state.dimensions.x * LAYER_MAX + 1 ) * m_map_state.dimensions.y * 4 / 2 // + 1 for overdraw column
	);
	NEW( m_mesh_terrain_data, types::mesh::Data, // data mesh has only one layer and no overdraw column
		m_map_state.dimensions.x * m_map_state.dimensions.y * 5 / 2,
		m_map_state.dimensions.x * m_map_state.dimensions.y * 4 / 2
	);
	
	// TODO: refactor?
	m_map_state.terrain_texture = m_textures.terrain;
	m_map_state.ter1_pcx = m_textures.source.ter1_pcx;
}

void Map::InitTerrainActor() {
	Log( "Spawning terrain actor" );
	
	ASSERT( !m_actors.terrain, "terrain actor already exists" );
	
	NEWV( terrain, actor::Mesh, "MapTerrain", m_mesh_terrain );
		terrain->SetTexture( m_textures.terrain );
		terrain->SetPosition( Map::s_consts.map_position );
		terrain->SetAngle( Map::s_consts.map_rotation );
		terrain->SetDataMesh( m_mesh_terrain_data );
	
	NEW( m_actors.terrain, actor::Instanced, terrain );
	m_scene->AddActor( m_actors.terrain );
}

void Map::GenerateActors() {
	ASSERT( m_tiles, "map tiles not set" );
	m_tiles->Validate();
	
	Log( "Initializing map" );
	
	m_map_state = {};
	
	m_map_state.dimensions = {
		m_tiles->GetWidth(),
		m_tiles->GetHeight()
	};
	m_map_state.coord = {
		- ( Map::s_consts.tile.scale.x * ( m_map_state.dimensions.x + 1 ) / 4 - Map::s_consts.tile.radius.x ),
		- ( Map::s_consts.tile.scale.y * ( m_map_state.dimensions.y + 1 ) / 4 - Map::s_consts.tile.radius.y )
	};
	m_map_state.variables.texture_scaling = {
		1.0f / Map::s_consts.tile_texture.dimensions.x / ( m_map_state.dimensions.x + 1 ), // + 1 for overdraw column
		1.0f / Map::s_consts.tile_texture.dimensions.y / m_map_state.dimensions.y / LAYER_MAX
	};
	
	LinkTileStates();
	
	if ( m_actors.terrain ) {
		m_scene->RemoveActor( m_actors.terrain );
		DELETE( m_actors.terrain );
	}

	InitTextureAndMesh();
	
	RunModulePasses( m_modules );
	
	for ( auto& c : m_map_state.copy_from_after ) {
		m_textures.terrain->AddFrom( m_textures.terrain, c.mode, c.tx1_from, c.ty1_from, c.tx2_from, c.ty2_from, c.tx_to, c.ty_to, c.rotate, c.alpha, m_random, c.perlin );
	}
	m_map_state.copy_from_after.clear();
	
	RunModulePasses( m_modules_deferred );
	
	m_mesh_terrain->Finalize();
	m_mesh_terrain_data->Finalize();
	
	// fix normals
	
	Log( "Fixing normals" );
	
	std::vector< mesh::Mesh::index_t > v, vtmp;
	
	// to prevent reallocations
	vtmp.reserve( 4 );
	v.reserve( 8 );
	
	for ( auto y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
		for ( auto x = y & 1 ; x < m_map_state.dimensions.x ; x += 2 ) {
			auto* tile = m_tiles->At( x, y );
			auto* ts = GetTileState( x, y );
			
			// combine at left vertex
			#define x( _lt ) { \
				ts->layers[ _lt ].indices.left, \
				ts->NW->layers[ _lt ].indices.bottom, \
				ts->W->layers[ _lt ].indices.right, \
				ts->SW->layers[ _lt ].indices.top, \
			}

			v = x( LAYER_LAND );
			
			if ( tile->is_water_tile || tile->W->is_water_tile || tile->NW->is_water_tile ) {
				vtmp = x( LAYER_WATER );
				v.insert( v.end(), vtmp.begin(), vtmp.end() );
			}
			
			m_mesh_terrain->CombineNormals( v );
			
			#undef x
		}
	}
	
	// average center normals
	for ( auto y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
		for ( auto x = y & 1 ; x < m_map_state.dimensions.x ; x += 2 ) {
			auto* tile = m_tiles->At( x, y );
			auto* ts = GetTileState( x, y );
			
			m_mesh_terrain->SetVertexNormal( ts->layers[ LAYER_LAND ].indices.center, (
				m_mesh_terrain->GetVertexNormal( ts->layers[ LAYER_LAND ].indices.left ) +
				m_mesh_terrain->GetVertexNormal( ts->layers[ LAYER_LAND ].indices.top ) +
				m_mesh_terrain->GetVertexNormal( ts->layers[ LAYER_LAND ].indices.right ) +
				m_mesh_terrain->GetVertexNormal( ts->layers[ LAYER_LAND ].indices.bottom )
			) / 4 );
		}
	}
	
	m_map_state.range.min = GetTileState( 0, 0 )->coord;
	m_map_state.range.max = GetTileState( m_map_state.dimensions.x - 1, m_map_state.dimensions.y - 1 )->coord;
	m_map_state.range.percent_to_absolute.x.SetRange({
		{ 0.0f, 1.0f },
		{ m_map_state.range.min.x - Map::s_consts.tile.radius.x , m_map_state.range.max.x + Map::s_consts.tile.radius.x }
	});
	m_map_state.range.percent_to_absolute.y.SetRange({
		{ 0.0f, 1.0f },
		{ m_map_state.range.min.y - Map::s_consts.tile.radius.y, m_map_state.range.max.y + Map::s_consts.tile.radius.y }
	});
	
	InitTerrainActor();
	
	m_current_tile = nullptr;
	m_current_ts = nullptr;
}

const Map::tile_texture_info_t Map::GetTileTextureInfo( const texture_variants_type_t type, const Tile* tile, const tile_grouping_criteria_t criteria, const Tile::feature_t feature ) const {
	ASSERT( m_current_ts, "GetTileTextureInfo called outside of tile generation" );
	Map::tile_texture_info_t info;
	
	bool matches[ 16 ];
	uint8_t idx = 0;
	for ( uint8_t i = 0 ; i < 2 ; i++ ) {
		for ( auto& t : tile->neighbours ) {
			switch ( criteria ) {
				case TG_MOISTURE: {
					matches[ idx++ ] = t->moisture >= tile->moisture;
					break;
				}
				case TG_FEATURE: {
					matches[ idx++ ] = (
						( t->features & feature ) == ( tile->features & feature ) ||
						( type == TVT_RIVERS && !tile->is_water_tile && t->is_water_tile ) // rivers end in sea
					);
					break;
				}
				default: {
					ASSERT( false, "invalid tile grouping criteria" );
				}
			}
		}
	}
	
	std::unordered_map< uint8_t, std::vector< uint8_t > > possible_variants = {}; // variant -> rotations
	
	const auto& variants = m_texture_variants.find( type );
	ASSERT( variants != m_texture_variants.end(), "texture variants for " + std::to_string( type ) + " not calculated" );
	
	for ( info.rotate_direction = 0 ; info.rotate_direction < 8 ; info.rotate_direction += 2 ) {
		uint8_t bitmask = 0;
		for ( uint8_t i = 0 ; i < 8 ; i++ ) {
			bitmask |= matches[ i + info.rotate_direction ] << i;
		}
		const auto& it = variants->second.find( bitmask );
		if ( it != variants->second.end() ) {
			for ( auto& v : it->second ) {
				possible_variants[ v ].push_back( info.rotate_direction );
			}
		}
	}
	
	if ( !possible_variants.empty() ) {
		// pick random variant if multiple
		auto it = possible_variants.begin();
		std::advance( it, m_random->GetUInt( 0, possible_variants.size() - 1 ) );
		info.texture_variant = it->first;
		info.rotate_direction = it->second[ m_random->GetUInt( 0, it->second.size() - 1 ) ] / 2;
	}
	else {
		ASSERT( false, "could not find texture variant" );
	}
	
	info.texture_flags = Texture::AM_DEFAULT;
	
	if ( type == TVT_TILES ) {
		if ( info.texture_variant >= 14 ) {
			// no important edges so we can shuffle harder
			info.texture_flags |=
				Texture::AM_RANDOM_MIRROR_X | 
				Texture::AM_RANDOM_MIRROR_Y |
				Texture::AM_RANDOM_STRETCH_SHUFFLE
			;
		}
		else {
			info.texture_flags |= Texture::AM_RANDOM_STRETCH;
		}
	}
	return info;
}

void Map::AddTexture( const tile_layer_type_t tile_layer, const pcx_texture_coordinates_t& tc, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha, util::Perlin* perlin ) {
	ASSERT( m_current_ts, "AddTexture called outside of tile generation" );
	m_textures.terrain->AddFrom(
		m_textures.source.texture_pcx,
		mode,
		tc.x,
		tc.y,
		tc.x + Map::s_consts.tile_texture.dimensions.x - 1,
		tc.y + Map::s_consts.tile_texture.dimensions.y - 1,
		m_current_ts->tex_coord.x1,
		tile_layer * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + m_current_ts->tex_coord.y1,
		rotate,
		alpha,
		m_random,
		perlin
	);
};

void Map::CopyTextureFromLayer( const tile_layer_type_t tile_layer_from, const size_t tx_from, const size_t ty_from, const tile_layer_type_t tile_layer, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha, util::Perlin* perlin ) {
	ASSERT( m_current_ts, "CopyTextureFromLayer called outside of tile generation" );
	m_textures.terrain->AddFrom(
		m_textures.terrain,
		mode,
		tx_from,
		tile_layer_from * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ty_from,
		tx_from + Map::s_consts.tile_texture.dimensions.x - 1,
		tile_layer_from * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ty_from + Map::s_consts.tile_texture.dimensions.y - 1,
		m_current_ts->tex_coord.x1,
		tile_layer * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + m_current_ts->tex_coord.y1,
		rotate,
		alpha,
		m_random,
		perlin
	);
};

void Map::CopyTexture( const tile_layer_type_t tile_layer_from, const tile_layer_type_t tile_layer, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha, util::Perlin* perlin ) {
	ASSERT( m_current_ts, "CopyTexture called outside of tile generation" );
	CopyTextureFromLayer(
		tile_layer_from,
		m_current_ts->tex_coord.x1,
		m_current_ts->tex_coord.y1,
		tile_layer,
		mode,
		rotate,
		alpha,
		perlin
	);
};

void Map::CopyTextureDeferred( const tile_layer_type_t tile_layer_from, const size_t tx_from, const size_t ty_from,const tile_layer_type_t tile_layer, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha, util::Perlin* perlin ) {
	ASSERT( m_current_ts, "CopyTextureDeferred called outside of tile generation" );
	m_map_state.copy_from_after.push_back({
		mode,
		tx_from,
		tile_layer_from * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ty_from,
		tx_from + Map::s_consts.tile_texture.dimensions.x - 1,
		tile_layer_from * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ty_from + Map::s_consts.tile_texture.dimensions.y - 1,
		(size_t)m_current_ts->tex_coord.x1,
		tile_layer * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + (size_t)m_current_ts->tex_coord.y1,
		rotate,
		alpha,
		perlin
	});
};

void Map::GetTexture( Texture* dest_texture, const pcx_texture_coordinates_t& tc, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha ) {
	ASSERT( m_current_ts, "GetTexture called outside of tile generation" );
	ASSERT( dest_texture->m_width == Map::s_consts.tile_texture.dimensions.x, "tile dest texture width mismatch" );
	ASSERT( dest_texture->m_height == Map::s_consts.tile_texture.dimensions.y, "tile dest texture height mismatch" );
	dest_texture->AddFrom(
		m_textures.source.texture_pcx,
		mode,
		tc.x,
		tc.y,
		tc.x + Map::s_consts.tile_texture.dimensions.x - 1,
		tc.y + Map::s_consts.tile_texture.dimensions.y - 1,
		0,
		0,
		rotate,
		alpha,
		m_random
	);
}

void Map::GetTextureFromLayer( Texture* dest_texture, const tile_layer_type_t tile_layer, const size_t tx_from, const size_t ty_from, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha ) const {
	ASSERT( dest_texture->m_width == Map::s_consts.tile_texture.dimensions.x, "tile dest texture width mismatch" );
	ASSERT( dest_texture->m_height == Map::s_consts.tile_texture.dimensions.y, "tile dest texture height mismatch" );
	dest_texture->AddFrom(
		m_textures.terrain,
		mode,
		tx_from,
		tile_layer * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ty_from,
		tx_from + Map::s_consts.tile_texture.dimensions.x - 1,
		tile_layer * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ty_from + Map::s_consts.tile_texture.dimensions.y - 1,
		0,
		0,
		rotate,
		alpha,
		m_random
	);
}

void Map::SetTexture( const tile_layer_type_t tile_layer, tile_state_t* ts, Texture* src_texture, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha ) {
	ASSERT( m_current_ts, "SetTexture called outside of tile generation" );
	ASSERT( src_texture->m_width == Map::s_consts.tile_texture.dimensions.x, "tile src texture width mismatch" );
	ASSERT( src_texture->m_height == Map::s_consts.tile_texture.dimensions.y, "tile src texture height mismatch" );
	m_textures.terrain->AddFrom(
		src_texture,
		mode,
		0,
		0,
		Map::s_consts.tile_texture.dimensions.x - 1,
		Map::s_consts.tile_texture.dimensions.x - 1,
		ts->tex_coord.x1,
		tile_layer * m_map_state.dimensions.y * Map::s_consts.tile_texture.dimensions.y + ts->tex_coord.y1,
		rotate,
		alpha,
		m_random
	);
}

void Map::SetTexture( const tile_layer_type_t tile_layer, Texture* src_texture, const Texture::add_flag_t mode, const uint8_t rotate, const float alpha ) {
	SetTexture( tile_layer, m_current_ts, src_texture, mode, rotate, alpha );
}

void Map::SetMinimapColor( const Color& color ) {
	ASSERT( m_current_tile, "SetMinimapColor called outside of tile generation" );
	ASSERT( m_textures.minimap, "minimap texture does not exist" );
	Log( "Setting minimap color for " + m_current_tile->coord.ToString() + " to " + color.ToString() );
	// convert from SMAC to packed coordinates
	auto coord = m_current_tile->coord;
	coord.x /= 2;
	if ( coord.y % 2 ) {
		coord.x += 1;
	}
	coord.y /= 2;
	m_textures.minimap->SetPixel( coord.x, coord.y, color );
}

const Texture* Map::GetTerrainTexture() const {
	ASSERT( m_textures.terrain, "terrain texture requested but not initialized" );
	return m_textures.terrain;
}

Map::tile_state_t* Map::GetTileState( const size_t x, const size_t y ) const {
	ASSERT( x < m_map_state.dimensions.x, "tile state x overflow" );
	ASSERT( y < m_map_state.dimensions.y, "tile state y overflow" );
	ASSERT( ( x % 2 ) == ( y % 2 ), "tile state axis oddity differs" );
	return &m_tile_states[ y * m_map_state.dimensions.x + x / 2 ];
}

Map::tile_state_t* Map::GetTileState( const Tile* tile ) const {
	return GetTileState( tile->coord.x, tile->coord.y );
}

const Map::map_state_t* Map::GetMapState() const {
	return &m_map_state;
}

Scene* Map::GetScene() const {
	return m_scene;
}

scene::actor::Instanced* Map::GenerateTerrainSpriteActor( const std::string& name, const pcx_texture_coordinates_t& tex_coords ) const {
	NEWV(
		sprite,
		scene::actor::Sprite,
		name,
		{
			Map::s_consts.tile.scale.x,
			Map::s_consts.tile.scale.y * Map::s_consts.sprite.y_scale
		},
		m_textures.source.ter1_pcx,
		{
			{
				(float) 1.0f / m_textures.source.ter1_pcx->m_width * ( tex_coords.x + 1 ) + Map::s_consts.sprite.zoom,
				(float) 1.0f / m_textures.source.ter1_pcx->m_height * ( tex_coords.y + 1 ) + Map::s_consts.sprite.zoom
			},
			{
				(float) 1.0f / m_textures.source.ter1_pcx->m_width * ( tex_coords.x + Map::s_consts.tc.ter1_pcx.dimensions.x - 2 ) - Map::s_consts.sprite.zoom,
				(float) 1.0f / m_textures.source.ter1_pcx->m_height * ( tex_coords.y + Map::s_consts.tc.ter1_pcx.dimensions.y - 2 ) - Map::s_consts.sprite.zoom
			}
		}
	);
	NEWV( instanced, scene::actor::Instanced, sprite );
	return instanced;
}

const size_t Map::GetWidth() const {
	return m_map_state.dimensions.x;
}

const size_t Map::GetHeight() const {
	return m_map_state.dimensions.y;
}

Random* Map::GetRandom() const {
	return m_random;
}

const Map::tile_info_t Map::GetTileAt( const size_t tile_x, const size_t tile_y ) const {
	ASSERT( m_tiles, "tiles not set" );
	tile_info_t result;
	return { m_tiles->At( tile_x, tile_y ), GetTileState( tile_x, tile_y ), &m_map_state };
}

void Map::CancelTileAtRequest() {
	ASSERT( m_tile_at_request_id, "tileat request not found" );
	m_actors.terrain->GetMeshActor()->CancelDataRequest( m_tile_at_request_id );
	m_tile_at_request_id = 0;
}	

void Map::GetTileAtScreenCoords( const size_t screen_x, const size_t screen_inverse_y ) {
	if ( m_tile_at_request_id ) {
		m_actors.terrain->GetMeshActor()->CancelDataRequest( m_tile_at_request_id );
	}
	m_tile_at_request_id = m_actors.terrain->GetMeshActor()->GetDataAt( screen_x, screen_inverse_y );
}

Map::tile_info_t Map::GetTileAtScreenCoordsResult() {
	if ( m_tile_at_request_id ) {
		auto result = m_actors.terrain->GetMeshActor()->GetDataResponse( m_tile_at_request_id );
		if ( result.first ) {
			m_tile_at_request_id = 0;
			
			if ( result.second ) {
				auto data = *result.second;
				if ( data ) { // some tile was clicked
				
					data--; // we used +1 increment to differentiate 'tile at 0,0' from 'no tiles'

					size_t tile_x = data % m_map_state.dimensions.x;
					size_t tile_y = data / m_map_state.dimensions.x;

					return GetTileAt( tile_x, tile_y );
				}
			}
		}
	}
	
	// no data
	return { nullptr, nullptr, nullptr };
}

void Map::GetMinimapTexture( scene::Camera* camera, const Vec2< size_t > texture_dimensions ) {
	if ( m_minimap_texture_request_id ) {
		Log( "Canceling minimap texture request" );
		m_actors.terrain->GetMeshActor()->CancelDataRequest( m_minimap_texture_request_id );
	}
	Log( "Requesting minimap texture" );
	m_minimap_texture_request_id = m_actors.terrain->GetMeshActor()->CaptureToTexture( camera, texture_dimensions );
}

Texture* Map::GetMinimapTextureResult() {
	if ( m_minimap_texture_request_id ) {
		auto result = m_actors.terrain->GetMeshActor()->GetCaptureToTextureResponse( m_minimap_texture_request_id );
		if ( result ) {
			Log( "Received minimap texture" );
			m_minimap_texture_request_id = 0;
			return result;
		}
	}
	// no texture (yet)
	return nullptr;
}

void Map::CalculateTextureVariants( const texture_variants_type_t type, const texture_variants_rules_t& rules ) {
	ASSERT( m_texture_variants.find( type ) == m_texture_variants.end(), "texture variants for " + std::to_string( type ) + " already calculated" );
	auto& variants = m_texture_variants[ type ];
	for ( uint16_t bitmask = 0 ; bitmask < 256 ; bitmask++ ) {
		ssize_t i = rules.size() - 1;
		auto& v = variants[ bitmask ] = {};
		while ( i >= 0 ) {
			const auto& rule = rules.at( i );
			if ( ( bitmask & rule.checked_bits ) == ( rule.bitmask & rule.checked_bits ) ) {
				v.push_back( i );
			}
			i--;
		}
	}
}

const Buffer Map::Serialize() const {
	Buffer buf;
	
	ASSERT( m_tiles, "tiles not set, can't serialize" );
	
	buf.WriteString( m_map_state.Serialize().ToString() );
	
	buf.WriteString( m_tiles->Serialize().ToString() );
	for ( auto y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
		for ( auto x = 0; x < m_map_state.dimensions.x ; x++ ) {
			if ( ( x % 2 ) != ( y % 2 ) ) {
				continue;
			}
			buf.WriteString( GetTileState( x, y )->Serialize().ToString() );
		}
	}
	
	buf.WriteString( m_textures.terrain->Serialize().ToString() );
	buf.WriteString( m_textures.minimap->Serialize().ToString() );
	buf.WriteString( m_mesh_terrain->Serialize().ToString() );
	buf.WriteString( m_mesh_terrain_data->Serialize().ToString() );
	
	return buf;
}

const Buffer Map::map_state_t::Serialize() const {
	Buffer buf;
	
	buf.WriteVec2f( coord );
	buf.WriteVec2f( range.min );
	buf.WriteVec2f( range.max );
	buf.WriteVec2u( dimensions );
	buf.WriteVec2f( variables.texture_scaling );
	
	return buf;
}

const Buffer Map::tile_state_t::Serialize() const {
	Buffer buf;
	
	buf.WriteVec2f( coord );
	buf.WriteFloat( tex_coord.x );
	buf.WriteFloat( tex_coord.y );
	buf.WriteFloat( tex_coord.x1 );
	buf.WriteFloat( tex_coord.y1 );
	buf.WriteFloat( tex_coord.x2 );
	buf.WriteFloat( tex_coord.y2 );
	buf.WriteString( elevations.Serialize().ToString() );
	buf.WriteInt( LAYER_MAX );
	for ( auto i = 0 ; i < LAYER_MAX ; i++ ) {
		buf.WriteString( layers[ i ].Serialize().ToString() );
	}
	buf.WriteString( overdraw_column.coords.Serialize().ToString() );
	buf.WriteString( overdraw_column.indices.Serialize().ToString() );
	buf.WriteString( data_mesh.coords.Serialize().ToString() );
	buf.WriteString( data_mesh.indices.Serialize().ToString() );
	buf.WriteBool( has_water );
	buf.WriteBool( is_coastline_corner );
	buf.WriteString( moisture_original->Serialize().ToString() );
	if ( river_original ) {
		buf.WriteBool( true );
		buf.WriteString( river_original->Serialize().ToString() );
	}
	else {
		buf.WriteBool( false );
	}
	
	buf.WriteInt( sprites.size() );
	for ( auto& a : sprites ) {
#ifdef DEBUG
		if ( !a.actor ) {
			THROW( "sprite actor not set" );
		}
#endif
		buf.WriteVec2u( a.tex_coords );
		buf.WriteString( a.actor->GetLocalName() );
		buf.WriteString( a.actor->Serialize().ToString() );
		buf.WriteString( a.actor->GetSpriteActor()->Serialize().ToString() );
	}
	
	return buf;
}

const Buffer Map::tile_elevations_t::Serialize() const {
	Buffer buf;
	
	buf.WriteInt( center );
	buf.WriteInt( left );
	buf.WriteInt( top );
	buf.WriteInt( right );
	buf.WriteInt( bottom );
	
	return buf;
}

const Buffer Map::tile_layer_t::Serialize() const {
	Buffer buf;
	
	buf.WriteString( indices.Serialize().ToString() );
	buf.WriteString( coords.Serialize().ToString() );
	buf.WriteString( tex_coords.Serialize().ToString() );
	buf.WriteString( colors.Serialize().ToString() );
	buf.WriteVec2f( texture_stretch );
	buf.WriteBool( texture_stretch_at_edges );
	
	return buf;
}

const Buffer Map::tile_indices_t::Serialize() const {
	Buffer buf;
	
	buf.WriteInt( center );
	buf.WriteInt( left );
	buf.WriteInt( top );
	buf.WriteInt( right );
	buf.WriteInt( bottom );
	
	return buf;
}

const Buffer Map::tile_vertices_t::Serialize() const {
	Buffer buf;
	
	buf.WriteVec3( center );
	buf.WriteVec3( left );
	buf.WriteVec3( top );
	buf.WriteVec3( right );
	buf.WriteVec3( bottom );
	
	return buf;
}

const Buffer Map::tile_tex_coords_t::Serialize() const {
	Buffer buf;
	
	buf.WriteVec2f( center );
	buf.WriteVec2f( left );
	buf.WriteVec2f( top );
	buf.WriteVec2f( right );
	buf.WriteVec2f( bottom );
	
	return buf;
}

const Buffer Map::tile_colors_t::Serialize() const {
	Buffer buf;
	
	buf.WriteColor( center );
	buf.WriteColor( left );
	buf.WriteColor( top );
	buf.WriteColor( right );
	buf.WriteColor( bottom );
	
	return buf;
}

void Map::Unserialize( Buffer buf ) {
	ASSERT( m_tiles, "tiles not set, can't unserialize" );
	
	m_map_state.Unserialize( Buffer( buf.ReadString() ) );
	
	m_tiles->Unserialize( buf.ReadString() );

	ASSERT( m_tiles->GetWidth() == m_map_state.dimensions.x, "tiles width doesn't match map width ( " + std::to_string( m_tiles->GetWidth() ) + " != " + std::to_string( m_map_state.dimensions.x ) + " )" );
	ASSERT( m_tiles->GetHeight() == m_map_state.dimensions.y, "tiles height doesn't match map height ( " + std::to_string( m_tiles->GetHeight() ) + " != " + std::to_string( m_map_state.dimensions.y ) + " )" );

	if ( m_tile_states ) {
		free( m_tile_states );
		m_tile_states = nullptr;
	}
	
	LinkTileStates();
	
	for ( auto y = 0 ; y < m_map_state.dimensions.y ; y++ ) {
		for ( auto x = 0; x < m_map_state.dimensions.x ; x++ ) {
			if ( ( x % 2 ) != ( y % 2 ) ) {
				continue;
			}
			GetTileState( x, y )->Unserialize( this, buf.ReadString() );
		}
	}
	
	InitTextureAndMesh();
	
	m_textures.terrain->Unserialize( buf.ReadString() );
	m_textures.minimap->Unserialize( buf.ReadString() );
	m_mesh_terrain->Unserialize( buf.ReadString() );
	m_mesh_terrain_data->Unserialize( buf.ReadString() );
	
	InitTerrainActor();
	
	m_map_state.range.percent_to_absolute.x.SetRange({
		{ 0.0f, 1.0f },
		{ m_map_state.range.min.x, m_map_state.range.max.x }
	});
	m_map_state.range.percent_to_absolute.y.SetRange({
		{ 0.0f, 1.0f },
		{ m_map_state.range.min.y, m_map_state.range.max.y }
	});
}

void Map::map_state_t::Unserialize( Buffer buf ) {
	coord = buf.ReadVec2f();
	range.min = buf.ReadVec2f();
	range.max = buf.ReadVec2f();
	dimensions = buf.ReadVec2u();
	variables.texture_scaling = buf.ReadVec2f();
}

void Map::tile_state_t::Unserialize( const Map* map, Buffer buf ) {
	coord = buf.ReadVec2f();
	tex_coord.x = buf.ReadFloat();
	tex_coord.y = buf.ReadFloat();
	tex_coord.x1 = buf.ReadFloat();
	tex_coord.y1 = buf.ReadFloat();
	tex_coord.x2 = buf.ReadFloat();
	tex_coord.y2 = buf.ReadFloat();
	elevations.Unserialize( buf.ReadString() );
	if ( (tile_layer_type_t)buf.ReadInt() != LAYER_MAX ) {
		THROW( "LAYER_MAX mismatch" );
	}
	for ( auto i = 0 ; i < LAYER_MAX ; i++ ) {
		layers[i].Unserialize( buf.ReadString() );
	}
	overdraw_column.coords.Unserialize( buf.ReadString() );
	overdraw_column.indices.Unserialize( buf.ReadString() );
	data_mesh.coords.Unserialize( buf.ReadString() );
	data_mesh.indices.Unserialize( buf.ReadString() );
	has_water = buf.ReadBool();
	is_coastline_corner = buf.ReadBool();
	
	const auto w = Map::s_consts.tile_texture.dimensions.x;
	const auto h = Map::s_consts.tile_texture.dimensions.y;
	
	NEW( moisture_original, Texture, "MoistureOriginal", w, h );
	moisture_original->Unserialize( buf.ReadString() );
	const bool has_river_original = buf.ReadBool();
	if ( has_river_original ) {
		NEW( river_original, Texture, "RiverOriginal", w, h );
		river_original->Unserialize( buf.ReadString() );
	}
	
	const size_t sprites_count = buf.ReadInt();
	sprites.clear();
	for ( size_t i = 0 ; i < sprites_count ; i++ ) {
		sprite_t sprite;
		sprite.tex_coords = buf.ReadVec2u();
		sprite.actor = map->GenerateTerrainSpriteActor( buf.ReadString(), sprite.tex_coords );
		sprite.actor->Unserialize( buf.ReadString() );
		sprite.actor->GetSpriteActor()->Unserialize( buf.ReadString() );
		map->m_scene->AddActor( sprite.actor );
		sprites.push_back( sprite );
	}

}

void Map::tile_elevations_t::Unserialize( Buffer buf ) {
	center = buf.ReadInt();
	left = buf.ReadInt();
	top = buf.ReadInt();
	right = buf.ReadInt();
	bottom = buf.ReadInt();
}

void Map::tile_layer_t::Unserialize( Buffer buf ) {
	indices.Unserialize( buf.ReadString() );
	coords.Unserialize( buf.ReadString() );
	tex_coords.Unserialize( buf.ReadString() );
	colors.Unserialize( buf.ReadString() );
	texture_stretch = buf.ReadVec2f();
	texture_stretch_at_edges = buf.ReadBool();
}

void Map::tile_indices_t::Unserialize( Buffer buf ) {
	center = buf.ReadInt();
	left = buf.ReadInt();
	top = buf.ReadInt();
	right = buf.ReadInt();
	bottom = buf.ReadInt();
}

void Map::tile_vertices_t::Unserialize( Buffer buf ) {
	center = buf.ReadVec3();
	left = buf.ReadVec3();
	top = buf.ReadVec3();
	right = buf.ReadVec3();
	bottom = buf.ReadVec3();
}

void Map::tile_tex_coords_t::Unserialize( Buffer buf ) {
	center = buf.ReadVec2f();
	left = buf.ReadVec2f();
	top = buf.ReadVec2f();
	right = buf.ReadVec2f();
	bottom = buf.ReadVec2f();
}

void Map::tile_colors_t::Unserialize( Buffer buf ) {
	center = buf.ReadColor();
	left = buf.ReadColor();
	top = buf.ReadColor();
	right = buf.ReadColor();
	bottom = buf.ReadColor();
}


}
}
}
