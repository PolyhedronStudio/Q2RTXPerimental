/********************************************************************
*
*
*	ServerGame: Standard Container Type Aliases
*
*	Type aliases mapping C++ standard library containers to the server-game
*	engine-backed zone allocator family (svg_zone_allocator_t).
*
*
********************************************************************/
#pragma once

#include <vector>
#include <deque>
#include <list>
#include <forward_list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <stack>
#include <string>

// Include zone allocator header defining svg_zone_allocator_t.
#include "svgame/memory/svg_zone_allocator.h"

/**
*	@brief	Standard vector backed by the server-game zone allocator.
*	@tparam	T	Element type.
*	@tparam	Tag	Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
using svg_vector_t = std::vector<T, svg_zone_allocator_t<T, Tag>>;

/**
*	@brief	Standard deque backed by the server-game zone allocator.
*	@tparam	T	Element type.
*	@tparam	Tag	Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
using svg_deque_t = std::deque<T, svg_zone_allocator_t<T, Tag>>;

/**
*	@brief	Standard doubly-linked list backed by the server-game zone allocator.
*	@tparam	T	Element type.
*	@tparam	Tag	Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
using svg_list_t = std::list<T, svg_zone_allocator_t<T, Tag>>;

/**
*	@brief	Standard singly-linked forward list backed by the server-game zone allocator.
*	@tparam	T	Element type.
*	@tparam	Tag	Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
using svg_forward_list_t = std::forward_list<T, svg_zone_allocator_t<T, Tag>>;

/**
*	@brief	Standard ordered map backed by the server-game zone allocator.
*	@tparam	Key		Key type.
*	@tparam	Value	Mapped value type.
*	@tparam	Compare	Comparator predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Value, typename Compare = std::less<Key>, memtag_t Tag = TAG_SVGAME>
using svg_map_t = std::map<Key, Value, Compare, svg_zone_allocator_t<std::pair<const Key, Value>, Tag>>;

/**
*	@brief	Standard ordered multimap backed by the server-game zone allocator.
*	@tparam	Key		Key type.
*	@tparam	Value	Mapped value type.
*	@tparam	Compare	Comparator predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Value, typename Compare = std::less<Key>, memtag_t Tag = TAG_SVGAME>
using svg_multimap_t = std::multimap<Key, Value, Compare, svg_zone_allocator_t<std::pair<const Key, Value>, Tag>>;

/**
*	@brief	Standard ordered set backed by the server-game zone allocator.
*	@tparam	Key		Element key type.
*	@tparam	Compare	Comparator predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Compare = std::less<Key>, memtag_t Tag = TAG_SVGAME>
using svg_set_t = std::set<Key, Compare, svg_zone_allocator_t<Key, Tag>>;

/**
*	@brief	Standard ordered multiset backed by the server-game zone allocator.
*	@tparam	Key		Element key type.
*	@tparam	Compare	Comparator predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Compare = std::less<Key>, memtag_t Tag = TAG_SVGAME>
using svg_multiset_t = std::multiset<Key, Compare, svg_zone_allocator_t<Key, Tag>>;

/**
*	@brief	Standard unordered hash map backed by the server-game zone allocator.
*	@tparam	Key		Key type.
*	@tparam	Value	Mapped value type.
*	@tparam	Hash	Hash functor.
*	@tparam	Equal	Key equality predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>, memtag_t Tag = TAG_SVGAME>
using svg_unordered_map_t = std::unordered_map<Key, Value, Hash, Equal, svg_zone_allocator_t<std::pair<const Key, Value>, Tag>>;

/**
*	@brief	Standard unordered hash multimap backed by the server-game zone allocator.
*	@tparam	Key		Key type.
*	@tparam	Value	Mapped value type.
*	@tparam	Hash	Hash functor.
*	@tparam	Equal	Key equality predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Value, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>, memtag_t Tag = TAG_SVGAME>
using svg_unordered_multimap_t = std::unordered_multimap<Key, Value, Hash, Equal, svg_zone_allocator_t<std::pair<const Key, Value>, Tag>>;

/**
*	@brief	Standard unordered hash set backed by the server-game zone allocator.
*	@tparam	Key		Element key type.
*	@tparam	Hash	Hash functor.
*	@tparam	Equal	Key equality predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>, memtag_t Tag = TAG_SVGAME>
using svg_unordered_set_t = std::unordered_set<Key, Hash, Equal, svg_zone_allocator_t<Key, Tag>>;

/**
*	@brief	Standard unordered hash multiset backed by the server-game zone allocator.
*	@tparam	Key		Element key type.
*	@tparam	Hash	Hash functor.
*	@tparam	Equal	Key equality predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename Key, typename Hash = std::hash<Key>, typename Equal = std::equal_to<Key>, memtag_t Tag = TAG_SVGAME>
using svg_unordered_multiset_t = std::unordered_multiset<Key, Hash, Equal, svg_zone_allocator_t<Key, Tag>>;

/**
*	@brief	Standard FIFO queue backed by the server-game zone allocator.
*	@tparam	T	Element type.
*	@tparam	Tag	Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
using svg_queue_t = std::queue<T, svg_deque_t<T, Tag>>;

/**
*	@brief	Standard LIFO stack backed by the server-game zone allocator.
*	@tparam	T	Element type.
*	@tparam	Tag	Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, memtag_t Tag = TAG_SVGAME>
using svg_stack_t = std::stack<T, svg_deque_t<T, Tag>>;

/**
*	@brief	Standard priority queue backed by the server-game zone allocator.
*	@tparam	T		Element type.
*	@tparam	Compare	Comparator predicate.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename T, typename Compare = std::less<T>, memtag_t Tag = TAG_SVGAME>
using svg_priority_queue_t = std::priority_queue<T, svg_vector_t<T, Tag>, Compare>;

/**
*	@brief	Standard basic string backed by the server-game zone allocator.
*	@tparam	CharT	Character type.
*	@tparam	Traits	Character traits.
*	@tparam	Tag		Engine zone memory tag (defaults to TAG_SVGAME).
**/
template <typename CharT, typename Traits = std::char_traits<CharT>, memtag_t Tag = TAG_SVGAME>
using svg_basic_string_t = std::basic_string<CharT, Traits, svg_zone_allocator_t<CharT, Tag>>;

//! Standard character string alias backed by TAG_SVGAME.
using svg_string_t = svg_basic_string_t<char>;
