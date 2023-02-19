#include "Coastlines.h"

namespace game {
namespace world {
namespace map {

Coastlines::Coastlines( Map* const map )
	: Module( map )
{
	NEW( m_perlin, util::Perlin, map->GetRandom()->GetUInt( 0, UINT32_MAX - 1 ) );
}

Coastlines::~Coastlines() {
	DELETE( m_perlin );
}

void Coastlines::GenerateTile( const Tile* tile, Map::tile_state_t* ts, Map::map_state_t* ms ) {
	
	// it is what it is
	
	float cw = 0.06f;
	float cwc = 1.5f;
	float tcw = cw * 2.0f;
	float tcwc = cwc * 0.85f;
	float tcww = tcw * Map::s_consts.pcx_texture_block.dimensions.x;
	float tcwh = tcw * Map::s_consts.pcx_texture_block.dimensions.y;
	const Texture::add_flag_t coastline_mode = Texture::AM_MERGE | Texture::AM_INVERT;
	struct coastline_corner_t {
		size_t msx = 0;
		size_t msy = 0;
		Texture::add_flag_t flags = 0;
		bool can_mirror = false;
		bool maybe_mirror_nw = false;
		bool maybe_mirror_ne = false;
		bool maybe_mirror_se = false;
		bool maybe_mirror_sw = false;
		Texture::add_flag_t mirror_mode;
	};
	std::vector< coastline_corner_t > coastline_corners = {};
	coastline_corner_t coastline_corner_tmp = {};
	
	if ( !tile->is_water_tile ) {
	
		if ( tile->W->is_water_tile || tile->NW->is_water_tile || tile->SW->is_water_tile ) {
			//ts->layers[ Map::LAYER_LAND ].colors.left = Map::s_consts.coastlines.coastline_tint;
			if ( tile->W->is_water_tile && ( tile->NW->is_water_tile || tile->SW->is_water_tile ) ) {
				ts->layers[ Map::LAYER_LAND ].coords.left.x += cw;
				if ( !tile->NW->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.left.y -= cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.left.y -= tcwh;
				}
				else if ( !tile->SW->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.left.y += cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.left.x += tcww;
				}
				else {
					ts->layers[ Map::LAYER_LAND ].coords.left.x += cw * cwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.left.y -= tcwh * tcwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.left.x += tcww * tcwc;
				}
			}
		}
		if ( tile->N->is_water_tile || tile->NW->is_water_tile || tile->NE->is_water_tile ) {
			//ts->layers[ Map::LAYER_LAND ].colors.top = Map::s_consts.coastlines.coastline_tint;
			if ( tile->N->is_water_tile && ( tile->NW->is_water_tile || tile->NE->is_water_tile ) ) {
				ts->layers[ Map::LAYER_LAND ].coords.top.y += cw;
				if ( !tile->NW->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.top.x -= cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.top.y += tcwh;
				}
				else if ( !tile->NE->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.top.x += cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.top.x += tcww;
				}
				else {
					ts->layers[ Map::LAYER_LAND ].coords.top.y += cw * cwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.top.x += tcww * tcwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.top.y += tcwh * tcwc;
				}
			}
		}
		if ( tile->E->is_water_tile || tile->NE->is_water_tile || tile->SE->is_water_tile ) {
			//ts->layers[ Map::LAYER_LAND ].colors.right = Map::s_consts.coastlines.coastline_tint;
			if ( tile->E->is_water_tile && ( tile->NE->is_water_tile || tile->SE->is_water_tile ) ) {
				ts->layers[ Map::LAYER_LAND ].coords.right.x -= cw;
				if ( !tile->NE->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.right.y -= cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.right.x -= tcww;
				}
				else if ( !tile->SE->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.right.y += cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.right.y += tcwh;
				}
				else {
					ts->layers[ Map::LAYER_LAND ].coords.right.x -= cw * cwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.right.x -= tcww * tcwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.right.y += tcwh * tcwc;
				}
			}
		}
		if ( tile->S->is_water_tile || tile->SW->is_water_tile || tile->SE->is_water_tile ) {
			//ts->layers[ Map::LAYER_LAND ].colors.bottom = Map::s_consts.coastlines.coastline_tint;
			if ( tile->S->is_water_tile && ( tile->SW->is_water_tile || tile->SE->is_water_tile ) ) {
				ts->layers[ Map::LAYER_LAND ].coords.bottom.y -= cw;
				if ( !tile->SW->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.bottom.x -= cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.bottom.x -= tcww;
				}
				else if ( !tile->SE->is_water_tile ) {
					ts->layers[ Map::LAYER_LAND ].coords.bottom.x += cw;
					ts->layers[ Map::LAYER_LAND ].tex_coords.bottom.y -= tcwh;
				}
				else {
					ts->layers[ Map::LAYER_LAND ].coords.bottom.y -= cw * cwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.bottom.x -= tcww * tcwc;
					ts->layers[ Map::LAYER_LAND ].tex_coords.bottom.y -= tcwh * tcwc;
				}
			}
		}
		ts->layers[ Map::LAYER_LAND ].tex_coords.center.x = ( ts->layers[ Map::LAYER_LAND ].tex_coords.left.x + ts->layers[ Map::LAYER_LAND ].tex_coords.top.x + ts->layers[ Map::LAYER_LAND ].tex_coords.right.x + ts->layers[ Map::LAYER_LAND ].tex_coords.bottom.x ) / 4;
		ts->layers[ Map::LAYER_LAND ].tex_coords.center.y = ( ts->layers[ Map::LAYER_LAND ].tex_coords.left.y + ts->layers[ Map::LAYER_LAND ].tex_coords.top.y + ts->layers[ Map::LAYER_LAND ].tex_coords.right.y + ts->layers[ Map::LAYER_LAND ].tex_coords.bottom.y ) / 4;
		
	}
	else { // is_water_tile
		
		// add perlin borders where needed
		
		coastline_corners.clear();
		
		if ( !tile->NW->is_water_tile && tile->coord.y > 0 ) {
			coastline_corner_tmp = {};
			coastline_corner_tmp.flags = Texture::AM_MIRROR_X | Texture::AM_PERLIN_LEFT;
			if ( tile->N->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_TOP;
			}
			if ( tile->W->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_BOTTOM;
			}
			if ( tile->coord.x > 0 ) {
				coastline_corner_tmp.msx = tile->coord.x - 1;
			}
			else {
				coastline_corner_tmp.msx = ms->dimensions.x - 1;
			}
			coastline_corner_tmp.msy = tile->coord.y - 1;
			coastline_corners.push_back( coastline_corner_tmp );
		}
		if ( !tile->NE->is_water_tile && tile->coord.y > 0 ) {
			coastline_corner_tmp = {};
			coastline_corner_tmp.flags |= Texture::AM_MIRROR_Y | Texture::AM_PERLIN_TOP;
			if ( tile->N->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_LEFT;
			}
			if ( tile->E->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_RIGHT;
			}
			if ( tile->coord.x < ms->dimensions.x - 1 ) {
				coastline_corner_tmp.msx = tile->coord.x + 1;
			}
			else {
				coastline_corner_tmp.msx = 0;
			}
			coastline_corner_tmp.msy = tile->coord.y - 1;
			coastline_corners.push_back( coastline_corner_tmp );
		}
		if ( !tile->SE->is_water_tile && tile->coord.y < ms->dimensions.y - 1 ) {
			coastline_corner_tmp = {};
			coastline_corner_tmp.flags |= Texture::AM_MIRROR_X | Texture::AM_PERLIN_RIGHT;
			if ( tile->E->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_TOP;
			}
			if ( tile->S->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_BOTTOM;
			}
			if ( tile->coord.x < ms->dimensions.x - 1 ) {
				coastline_corner_tmp.msx = tile->coord.x + 1;
			}
			else {
				coastline_corner_tmp.msx = 0;
			}
			coastline_corner_tmp.msy = tile->coord.y + 1;
			coastline_corners.push_back( coastline_corner_tmp );
		}
		if ( !tile->SW->is_water_tile && tile->coord.y < ms->dimensions.y - 1 ) {
			coastline_corner_tmp = {};
			coastline_corner_tmp.flags |= Texture::AM_MIRROR_Y | Texture::AM_PERLIN_BOTTOM;
			if ( tile->W->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_LEFT;
			}
			if ( tile->S->is_water_tile ) {
				coastline_corner_tmp.flags |= Texture::AM_PERLIN_CUT_RIGHT;
			}
			if ( tile->coord.x > 0 ) {
				coastline_corner_tmp.msx = tile->coord.x - 1;
			}
			else {
				coastline_corner_tmp.msx = ms->dimensions.x - 1;
			}
			coastline_corner_tmp.msy = tile->coord.y + 1;
			coastline_corners.push_back( coastline_corner_tmp );
		}

		for ( auto& c : coastline_corners ) {
			ASSERT( ( c.msx % 2 ) == ( c.msy % 2 ), "msx and msy oddity differs" );
			
			const float pb = m_map->GetRandom()->GetUInt( 1000, 100000 );
			
			// land part
			m_map->CopyTextureDeferred(
				Map::LAYER_LAND,
				c.msx * Map::s_consts.pcx_texture_block.dimensions.x,
				c.msy * Map::s_consts.pcx_texture_block.dimensions.y,
				Map::LAYER_WATER,
				Texture::AM_MERGE | c.flags,
				0,
				m_perlin->Noise( ( c.msx + tile->coord.x ) / 2, ( c.msy + tile->coord.y ) / 2, 0.5f ) / 2 + 0.5f,
				m_perlin
			);
		}
	}
	
	ts->layers[ Map::LAYER_LAND ].coords.center = {
		( ts->layers[ Map::LAYER_LAND ].coords.left.x + ts->layers[ Map::LAYER_LAND ].coords.right.x ) / 2,
		( ts->layers[ Map::LAYER_LAND ].coords.top.y + ts->layers[ Map::LAYER_LAND ].coords.bottom.y ) / 2,
		Map::s_consts.clampers.elevation_to_vertex_z.Clamp( ts->elevations.center )
	};
	
	// coast water texture
	/*if ( !tile->is_water_tile && (
		tile->W->is_water_tile ||
		tile->NW->is_water_tile ||
		tile->N->is_water_tile ||
		tile->NE->is_water_tile ||
		tile->E->is_water_tile ||
		tile->SE->is_water_tile ||
		tile->S->is_water_tile ||
		tile->SW->is_water_tile
	)) {
		m_map->AddTexture(
			Map::LAYER_WATER_SURFACE_EXTRA,
			Map::s_consts.pcx_textures.water[ 0 ],
			Texture::AM_DEFAULT,
			RandomRotate(),
			Map::s_consts.coastlines.coast_water_alpha
		);
		
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.center =
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.left =
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.top =
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.right =
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.bottom =
			Map::s_consts.coastlines.coastline_tint;
	}
	
	if ( tile->is_water_tile && (
		!tile->W->is_water_tile ||
		!tile->NW->is_water_tile ||
		!tile->N->is_water_tile ||
		!tile->NE->is_water_tile ||
		!tile->E->is_water_tile ||
		!tile->SE->is_water_tile ||
		!tile->S->is_water_tile ||
		!tile->SW->is_water_tile
	)) {
	
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.center = Map::s_consts.coastlines.coastline_tint * Map::s_consts.coastlines.coast_water_center_alpha_tint_mod;
		ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.center.value.alpha = Map::s_consts.coastlines.coast_water_center_alpha;

		if ( tile->W->is_water_tile && tile->NW->is_water_tile && tile->SW->is_water_tile ) {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.left.value.alpha = 0.0f;
		}
		else {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.left = Map::s_consts.coastlines.coastline_tint;
		}

		if ( tile->N->is_water_tile && tile->NW->is_water_tile && tile->NE->is_water_tile ) {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.top.value.alpha = 0.0f;
		}
		else {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.top = Map::s_consts.coastlines.coastline_tint;
		}

		if ( tile->E->is_water_tile && tile->NE->is_water_tile && tile->SE->is_water_tile ) {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.right.value.alpha = 0.0f;
		}
		else {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.right = Map::s_consts.coastlines.coastline_tint;
		}

		if ( tile->S->is_water_tile && tile->SW->is_water_tile && tile->SE->is_water_tile ) {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.bottom.value.alpha = 0.0f;
		}
		else {
			ts->layers[ Map::LAYER_WATER_SURFACE_EXTRA ].colors.bottom = Map::s_consts.coastlines.coastline_tint;
		}

		m_map->AddTexture(
			Map::LAYER_WATER_SURFACE_EXTRA,
			Map::s_consts.pcx_textures.water[ 0 ],
			Texture::AM_DEFAULT,
			RandomRotate(),
			Map::s_consts.coastlines.coast_water_alpha
		);
	}*/

	if ( ts->has_water ) {
		
		// corners on land tiles
		if ( ts->is_coastline_corner ) {

			// water under corner
			m_map->AddTexture(
				Map::LAYER_WATER_SURFACE_EXTRA,
				Map::s_consts.pcx_textures.water[ 1 ],
				Texture::AM_DEFAULT,
				RandomRotate()
			);

			// TODO: refactor?
			auto add_flags = Texture::AM_MERGE;
			if (
				tile->W->is_water_tile &&
				( tile->SW->is_water_tile || tile->coord.y == ms->dimensions.y - 1 ) &&
				( tile->NW->is_water_tile || tile->coord.y == 0 )
			) {
				add_flags |= Texture::AM_ROUND_LEFT;
			}
			if (
				( tile->N->is_water_tile || tile->coord.y <= 1 ) &&
				( tile->NW->is_water_tile || tile->coord.y == 0 ) &&
				( tile->NE->is_water_tile || tile->coord.y == 0 )
			) {
				add_flags |= Texture::AM_ROUND_TOP;
			}
			if (
				tile->E->is_water_tile &&
				( tile->SE->is_water_tile || tile->coord.y == ms->dimensions.y - 1 ) &&
				( tile->NE->is_water_tile || tile->coord.y == 0 )
			) {
				add_flags |= Texture::AM_ROUND_RIGHT;
			}
			if (
				( tile->S->is_water_tile || tile->coord.y <= ms->dimensions.y - 2 ) &&
				( tile->SE->is_water_tile || tile->coord.y == ms->dimensions.y - 1 ) &&
				( tile->SW->is_water_tile || tile->coord.y == ms->dimensions.y - 1 )
			) {
				add_flags |= Texture::AM_ROUND_BOTTOM;
			}

/*			// coastline tint
			if ( tile->W->is_water_tile || tile->SW->is_water_tile || tile->NW->is_water_tile ) {
				ts->layers[ Map::LAYER_WATER ].colors.left = Map::s_consts.coastlines.coastline_tint;
			}
			if ( tile->N->is_water_tile || tile->NW->is_water_tile || tile->NE->is_water_tile ) {
				ts->layers[ Map::LAYER_WATER ].colors.top = Map::s_consts.coastlines.coastline_tint;
			}
			if ( tile->E->is_water_tile || tile->SE->is_water_tile || tile->NE->is_water_tile ) {
				ts->layers[ Map::LAYER_WATER ].colors.right = Map::s_consts.coastlines.coastline_tint;
			}
			if ( tile->S->is_water_tile || tile->SW->is_water_tile || tile->SE->is_water_tile ) {
				ts->layers[ Map::LAYER_WATER ].colors.bottom = Map::s_consts.coastlines.coastline_tint;
			}
*/
			// copy texture from land
			m_map->CopyTexture( Map::LAYER_LAND, Map::LAYER_WATER, add_flags, 0 );

		}

		coastline_corners.clear();
		
		// corners on water tiles
		if ( tile->is_water_tile ) {

			if ( !tile->SW->is_water_tile && !tile->NW->is_water_tile ) {
				coastline_corner_tmp = {};
				coastline_corner_tmp.flags = Texture::AM_ROUND_LEFT;
				//ts->layers[ Map::LAYER_WATER ].colors.left = Map::s_consts.coastlines.coastline_tint;
				if ( !tile->W->is_water_tile ) {
					coastline_corner_tmp.can_mirror = true;
					if ( tile->coord.x >= 2 ) {
						coastline_corner_tmp.msx = tile->coord.x - 2;
					}
					else {
						coastline_corner_tmp.msx = ms->dimensions.x - 2 + tile->coord.x;
					}
					coastline_corner_tmp.msy = tile->coord.y;
				}
				else {
					coastline_corner_tmp.maybe_mirror_sw = true;
					coastline_corner_tmp.maybe_mirror_nw = true;
				}
				coastline_corners.push_back( coastline_corner_tmp );
			}

			if ( !tile->NW->is_water_tile && !tile->NE->is_water_tile ) {
				coastline_corner_tmp = {};
				coastline_corner_tmp.flags = Texture::AM_ROUND_TOP;
				//ts->layers[ Map::LAYER_WATER ].colors.top = Map::s_consts.coastlines.coastline_tint;
				if ( !tile->N->is_water_tile ) {
					if ( tile->coord.y >= 2 ) {
						coastline_corner_tmp.can_mirror = true;
						coastline_corner_tmp.msx = tile->coord.x;
						coastline_corner_tmp.msy = tile->coord.y - 2;
					}
				}
				else {
					coastline_corner_tmp.maybe_mirror_nw = true;
					coastline_corner_tmp.maybe_mirror_ne = true;
				}
				coastline_corners.push_back( coastline_corner_tmp );
			}
			if ( !tile->SE->is_water_tile && !tile->NE->is_water_tile ) {
				coastline_corner_tmp = {};
				coastline_corner_tmp.flags = Texture::AM_ROUND_RIGHT;
				//ts->layers[ Map::LAYER_WATER ].colors.right = Map::s_consts.coastlines.coastline_tint;
				if ( !tile->E->is_water_tile ) {
					coastline_corner_tmp.can_mirror = true;
					if ( tile->coord.x < ms->dimensions.x - 2 ) {
						coastline_corner_tmp.msx = tile->coord.x + 2;
					}
					else {
						coastline_corner_tmp.msx = tile->coord.x % 2;
					}
					coastline_corner_tmp.msy = tile->coord.y;
				}
				else {
					coastline_corner_tmp.maybe_mirror_se = true;
					coastline_corner_tmp.maybe_mirror_ne = true;
				}
				coastline_corners.push_back( coastline_corner_tmp );
			}
			if ( !tile->SE->is_water_tile && !tile->SW->is_water_tile ) {
				coastline_corner_tmp = {};
				coastline_corner_tmp.flags = Texture::AM_ROUND_BOTTOM;
				//ts->layers[ Map::LAYER_WATER ].colors.bottom = Map::s_consts.coastlines.coastline_tint;
				if ( !tile->S->is_water_tile ) {
					if ( tile->coord.y < ms->dimensions.y - 2 ) {
						coastline_corner_tmp.can_mirror = true;
						coastline_corner_tmp.msx = tile->coord.x;
						coastline_corner_tmp.msy = tile->coord.y + 2;
					}
				}
				else {
					coastline_corner_tmp.maybe_mirror_se = true;
					coastline_corner_tmp.maybe_mirror_sw = true;
				}
				coastline_corners.push_back( coastline_corner_tmp );
			}

			for ( auto& c : coastline_corners ) {

				if ( c.can_mirror ) {
					c.mirror_mode = Texture::AM_MIRROR_X | Texture::AM_MIRROR_Y;
				}

				if ( !c.can_mirror && tile->coord.y >= 1 ) {
					c.msy = tile->coord.y - 1;
					if ( !c.can_mirror && coastline_corner_tmp.maybe_mirror_nw ) {
						if ( tile->coord.x >= 1 ) {
							c.msx = tile->coord.x - 1;
						}
						else {
							c.msx = ms->dimensions.x - 1;
						}
						c.mirror_mode = Texture::AM_MIRROR_X;
						c.can_mirror = true;
					}
					if ( !c.can_mirror && coastline_corner_tmp.maybe_mirror_ne ) {
						if ( tile->coord.x < ms->dimensions.x - 1 ) {
							c.msx = tile->coord.x + 1;
						}
						else {
							c.msx = 1;
						}
						c.mirror_mode = Texture::AM_MIRROR_Y;
						c.can_mirror = true;
					}
				}
				if ( !c.can_mirror && tile->coord.y < ms->dimensions.y - 1 ) {
					c.msy = tile->coord.y + 1;
					if ( !c.can_mirror && coastline_corner_tmp.maybe_mirror_sw ) {
						if ( tile->coord.x >= 1 ) {
							c.msx = tile->coord.x - 1;
						}
						else {
							c.msx = ms->dimensions.x - 1;
						}
						c.mirror_mode = Texture::AM_MIRROR_X;
						c.can_mirror = true;
					}
					if ( !c.can_mirror && coastline_corner_tmp.maybe_mirror_se ) {
						if ( tile->coord.x < ms->dimensions.x - 1 ) {
							c.msx = tile->coord.x + 1;
						}
						else {
							c.msx = 1;
						}
						c.mirror_mode = Texture::AM_MIRROR_Y;
						c.can_mirror = true;
					}
				}
				
				if ( c.can_mirror ) {
					// mirror opposite tile
					m_map->CopyTextureDeferred( Map::LAYER_LAND, c.msx * Map::s_consts.pcx_texture_block.dimensions.x, c.msy * Map::s_consts.pcx_texture_block.dimensions.y, Map::LAYER_WATER, coastline_mode | c.flags | c.mirror_mode, 0 );
				}
				else {
					// use default texture
					m_map->AddTexture(
						Map::LAYER_WATER,
						Map::s_consts.pcx_textures.arid[ 0 ],
						coastline_mode | c.flags,
						RandomRotate()
					);
				}
			}
		}
	}
}

}
}
}
