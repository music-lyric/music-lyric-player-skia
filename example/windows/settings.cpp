#include "settings.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "imgui.h"
#include "theme.h"
#include "widgets.h"

namespace example {
	namespace {
		namespace config = music_lyric_player::rendering::config;
		namespace color  = theme::color;

		/**
		 * One editable leaf: the label shown beside the control, and the control itself.
		 * A control reads the resolved config, writes only what the user changed into the override store, and reports it through the edit.
		 */
		struct Field {
			std::string label;
			// Set only for a boolean leaf, so a titled group can promote it into its own header and fold the rest behind it.
			// Draws the card's title row and returns whether the body follows.
			std::function<bool(const char*, const config::Root&, config::Root&, SettingsEdit&)> master;
			std::function<void(const config::Root&, config::Root&, SettingsEdit&)>              draw;
		};

		/**
		 * A run of fields under an optional heading; an empty title keeps them inline under their section.
		 * A detail group folds behind a chevron and starts closed, the way the playground hides font and state blocks.
		 */
		struct Group {
			std::string        title;
			std::vector<Field> fields;
			bool               collapsible = false;
		};

		/**
		 * A collapsible block of groups that may nest further sections, mirroring the config tree.
		 */
		struct Section {
			std::string          title;
			std::vector<Group>   groups;
			std::vector<Section> children;
		};

		/**
		 * Returns the tone a control's text takes: a leaf the user has not overridden reads as a placeholder.
		 */
		ImVec4 valueTint(bool assigned) {
			return ImGui::ColorConvertU32ToFloat4(assigned ? color::text : color::textMuted);
		}

		/**
		 * Builds a switch bound to a boolean leaf, which a titled group may promote into its header.
		 */
		template <typename At>
		Field toggle(std::string label, At at) {
			auto draw = [at](const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
				const bool value = at(current).value();
				if (widgets::toggle("##value", value)) {
					at(overrides)  = !value;
					edit.changed   = true;
					edit.committed = true;
				}
			};
			auto master = [at](const char* title, const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
				const bool            value  = at(current).value();
				const widgets::Master header = widgets::groupMaster(title, value);
				if (header.toggled) {
					at(overrides)  = !value;
					edit.changed   = true;
					edit.committed = true;
				}
				return header.open;
			};
			return Field{std::move(label), std::move(master), std::move(draw)};
		}

		/**
		 * Builds a drag control bound to a numeric leaf, clamped to the range the schema documents.
		 */
		template <typename At>
		Field number(std::string label, At at, double min, double max, double step) {
			auto draw = [at, min, max, step](const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
				double value = at(current).value();
				ImGui::PushStyleColor(ImGuiCol_Text, valueTint(at(overrides).assigned()));
				if (ImGui::DragScalar("##value", ImGuiDataType_Double, &value, static_cast<float>(step), &min, &max, "%g", ImGuiSliderFlags_AlwaysClamp)) {
					at(overrides) = std::clamp(value, min, max);
					edit.changed  = true;
				}
				ImGui::PopStyleColor();
				// A drag reports every frame it moves, so the store reaches the disk only once the handle is released.
				if (ImGui::IsItemDeactivatedAfterEdit()) {
					edit.committed = true;
				}
			};
			return Field{std::move(label), nullptr, std::move(draw)};
		}

		/**
		 * The scratch a text field types into, kept alive between frames alongside the field itself.
		 */
		struct TextBuffer {
			std::array<char, 96> data{};
			bool                 editing = false;
		};

		/**
		 * Builds a text box bound to a string leaf.
		 * An untouched leaf shows the resolved value as a hint rather than as content, so it reads as inherited.
		 * The value is committed only once editing ends, since a half-typed color or length would otherwise reach the renderer.
		 */
		template <typename At>
		Field text(std::string label, At at) {
			const auto buffer = std::make_shared<TextBuffer>();
			auto       draw   = [at, buffer](const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
				// While the box is idle it follows the store, so an override applied elsewhere still shows up.
				if (!buffer->editing) {
					const bool assigned = at(overrides).assigned();
					std::snprintf(buffer->data.data(), buffer->data.size(), "%s", assigned ? at(overrides).value().c_str() : "");
				}
				ImGui::InputTextWithHint("##value", at(current).value().c_str(), buffer->data.data(), buffer->data.size());
				buffer->editing = ImGui::IsItemActive();
				// An emptied box cannot drop the override: a leaf has no way to be unset, and the renderer would keep the merged value anyway.
				if (ImGui::IsItemDeactivatedAfterEdit() && buffer->data[0] != '\0') {
					at(overrides)  = std::string(buffer->data.data());
					edit.changed   = true;
					edit.committed = true;
				}
			};
			return Field{std::move(label), nullptr, std::move(draw)};
		}

		/**
		 * One option of an enum select: the label shown and the value it sets.
		 */
		template <typename E>
		struct Option {
			const char* label;
			E           value;
		};

		/**
		 * Builds a select bound to an enum leaf.
		 */
		template <typename E, typename At>
		Field select(std::string label, At at, std::initializer_list<Option<E>> options) {
			const std::vector<Option<E>> values(options);
			auto                         draw = [at, values](const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
				const E                  value = at(current).value();
				std::vector<const char*> labels;
				int                      selected = -1;
				labels.reserve(values.size());
				for (std::size_t i = 0; i < values.size(); ++i) {
					labels.push_back(values[i].label);
					if (values[i].value == value) {
						selected = static_cast<int>(i);
					}
				}

				const int picked = widgets::select("##value", labels.data(), static_cast<int>(labels.size()), selected, at(overrides).assigned());
				if (picked >= 0) {
					at(overrides)  = values[static_cast<std::size_t>(picked)].value;
					edit.changed   = true;
					edit.committed = true;
				}
			};
			return Field{std::move(label), nullptr, std::move(draw)};
		}

		/**
		 * Builds a select bound to a string leaf, listing the values the renderer recognises.
		 * An empty value is the inherit slot, so it is labelled instead of shown blank.
		 */
		template <typename At>
		Field choice(std::string label, At at, std::vector<std::string> options) {
			auto draw = [at, values = std::move(options)](const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
				const std::string&       value = at(current).value();
				std::vector<const char*> labels;
				int                      selected = -1;
				labels.reserve(values.size());
				for (std::size_t i = 0; i < values.size(); ++i) {
					labels.push_back(values[i].empty() ? "Inherit" : values[i].c_str());
					if (values[i] == value) {
						selected = static_cast<int>(i);
					}
				}

				const int picked = widgets::select("##value", labels.data(), static_cast<int>(labels.size()), selected, at(overrides).assigned());
				if (picked >= 0) {
					at(overrides)  = values[static_cast<std::size_t>(picked)];
					edit.changed   = true;
					edit.committed = true;
				}
			};
			return Field{std::move(label), nullptr, std::move(draw)};
		}

		/**
		 * Returns the language tags the lyric model knows, optionally led by the inherit slot annotation rows offer.
		 */
		std::vector<std::string> languages(bool inherit) {
			std::vector<std::string> tags;
			if (inherit) {
				tags.emplace_back("");
			}
			for (const char* tag : {"zh-hans", "zh-hant", "en", "ja", "ko", "ru", "fr", "de", "es", "it", "pt"}) {
				tags.emplace_back(tag);
			}
			return tags;
		}

		void append(std::vector<Group>& groups, std::vector<Group> tail) {
			groups.insert(groups.end(), std::make_move_iterator(tail.begin()), std::make_move_iterator(tail.end()));
		}

		/**
		 * Builds the font group of a node that carries a common font config.
		 */
		template <typename At>
		Group fontGroup(At at) {
			std::vector<Field> fields;
			fields.push_back(text("Family", [at](auto& c) -> auto& { return at(c).font.family; }));
			fields.push_back(text("Size", [at](auto& c) -> auto& { return at(c).font.size; }));
			return Group{"Font", std::move(fields), true};
		}

		/**
		 * Builds the per-state color and opacity groups of a node that carries a common state style config.
		 * Only nodes that dim already-sung lines have a played state, so whether it exists is a compile-time choice.
		 */
		template <bool Played, typename At>
		std::vector<Group> styleGroups(At at) {
			std::vector<Group> groups;

			std::vector<Field> normal;
			normal.push_back(text("Color", [at](auto& c) -> auto& { return at(c).style.normal.color; }));
			normal.push_back(number("Opacity", [at](auto& c) -> auto& { return at(c).style.normal.opacity; }, 0.0, 1.0, 0.05));
			groups.push_back(Group{"Normal state", std::move(normal), true});

			std::vector<Field> active;
			active.push_back(text("Color", [at](auto& c) -> auto& { return at(c).style.active.color; }));
			active.push_back(number("Opacity", [at](auto& c) -> auto& { return at(c).style.active.opacity; }, 0.0, 1.0, 0.05));
			groups.push_back(Group{"Active state", std::move(active), true});

			if constexpr (Played) {
				std::vector<Field> played;
				played.push_back(text("Color", [at](auto& c) -> auto& { return at(c).style.played.color; }));
				played.push_back(number("Opacity", [at](auto& c) -> auto& { return at(c).style.played.opacity; }, 0.0, 1.0, 0.05));
				groups.push_back(Group{"Played state", std::move(played), true});
			}
			return groups;
		}

		/**
		 * Builds one scroll mode's group: the duration and easing every mode shares, then the mode's own fields.
		 */
		template <typename At>
		Group scrollGroup(std::string title, At at, std::vector<Field> extra) {
			std::vector<Field> fields;
			fields.push_back(number("Duration", [at](auto& c) -> auto& { return at(c).duration; }, 0.0, 2000.0, 10.0));
			fields.push_back(text("Easing", [at](auto& c) -> auto& { return at(c).easing; }));
			fields.insert(fields.end(), std::make_move_iterator(extra.begin()), std::make_move_iterator(extra.end()));
			return Group{std::move(title), std::move(fields), true};
		}

		/**
		 * Builds the cascade fields the ripple, directional and stagger modes share.
		 */
		template <typename At>
		std::vector<Field> cascadeFields(At at) {
			std::vector<Field> fields;
			fields.push_back(number("Range", [at](auto& c) -> auto& { return at(c).range; }, 1.0, 30.0, 1.0));
			fields.push_back(number("Step", [at](auto& c) -> auto& { return at(c).step; }, 1.0, 200.0, 1.0));
			return fields;
		}

		/**
		 * Builds the container section: the frame fill, its padding and the edge fade.
		 */
		Section containerSection() {
			std::vector<Field> frame;
			frame.push_back(text("Background", [](auto& c) -> auto& { return c.container.backgroundColor; }));
			frame.push_back(text("Padding X", [](auto& c) -> auto& { return c.container.paddingX; }));

			std::vector<Field> fade;
			fade.push_back(toggle("Enabled", [](auto& c) -> auto& { return c.container.fade.enabled; }));
			fade.push_back(text("Top", [](auto& c) -> auto& { return c.container.fade.top; }));
			fade.push_back(text("Bottom", [](auto& c) -> auto& { return c.container.fade.bottom; }));

			return Section{"Container", {Group{"", std::move(frame)}, Group{"Edge fade", std::move(fade)}}, {}};
		}

		/**
		 * Builds the layout and focus-effect sections.
		 */
		std::vector<Section> layoutSections() {
			std::vector<Field> layout;
			layout.push_back(select<config::layout::Align>("Align",
				[](auto& c) -> auto& { return c.layout.align; },
				{{"Left", config::layout::Align::Left}, {"Center", config::layout::Align::Center}, {"Right", config::layout::Align::Right}}));
			layout.push_back(text("Line gap", [](auto& c) -> auto& { return c.layout.gap; }));

			std::vector<Field> scale;
			scale.push_back(toggle("Enabled", [](auto& c) -> auto& { return c.effect.scale.enabled; }));
			scale.push_back(number("Min", [](auto& c) -> auto& { return c.effect.scale.min; }, 0.0, 2.0, 0.05));
			scale.push_back(number("Max", [](auto& c) -> auto& { return c.effect.scale.max; }, 0.0, 2.0, 0.05));

			std::vector<Field> blur;
			blur.push_back(toggle("Enabled", [](auto& c) -> auto& { return c.effect.blur.enabled; }));
			blur.push_back(number("Min", [](auto& c) -> auto& { return c.effect.blur.min; }, 0.0, 20.0, 0.1));
			blur.push_back(number("Max", [](auto& c) -> auto& { return c.effect.blur.max; }, 0.0, 20.0, 0.1));

			std::vector<Section> sections;
			sections.push_back(Section{"Layout", {Group{"", std::move(layout)}}, {}});
			sections.push_back(Section{"Effect", {Group{"Scale", std::move(scale)}, Group{"Blur", std::move(blur)}}, {}});
			return sections;
		}

		/**
		 * Builds the scroll section: the anchor, the active mode and every mode's own timing.
		 */
		Section scrollSection() {
			const auto smooth      = [](auto& c) -> auto& { return c.scroll.animation.smooth; };
			const auto ripple      = [](auto& c) -> auto& { return c.scroll.animation.ripple; };
			const auto directional = [](auto& c) -> auto& { return c.scroll.animation.directional; };
			const auto stagger     = [](auto& c) -> auto& { return c.scroll.animation.stagger; };

			std::vector<Field> head;
			head.push_back(number("Anchor", [](auto& c) -> auto& { return c.scroll.anchor; }, 0.0, 1.0, 0.01));
			head.push_back(select<config::scroll::Mode>("Mode",
				[](auto& c) -> auto& { return c.scroll.animation.mode; },
				{{"Smooth", config::scroll::Mode::Smooth},
					{"Ripple", config::scroll::Mode::Ripple},
					{"Directional", config::scroll::Mode::Directional},
					{"Stagger", config::scroll::Mode::Stagger}}));

			std::vector<Field> delay;
			delay.push_back(number("Delay", [smooth](auto& c) -> auto& { return smooth(c).delay; }, 0.0, 1000.0, 10.0));

			std::vector<Group> groups;
			groups.push_back(Group{"", std::move(head)});
			groups.push_back(scrollGroup("Smooth", smooth, std::move(delay)));
			groups.push_back(scrollGroup("Ripple", ripple, cascadeFields(ripple)));
			groups.push_back(scrollGroup("Directional", directional, cascadeFields(directional)));
			groups.push_back(scrollGroup("Stagger", stagger, cascadeFields(stagger)));
			return Section{"Scroll", std::move(groups), {}};
		}

		/**
		 * Builds the normal line's main content section: the karaoke font and styles plus every word animation.
		 */
		Section mainSection() {
			const auto syllable  = [](auto& c) -> auto& { return c.line.normal.main.syllable; };
			const auto animation = [](auto& c) -> auto& { return c.line.normal.main.syllable.word.animation; };
			const auto emphasize = [](auto& c) -> auto& { return c.line.normal.main.syllable.word.animation.emphasize; };

			std::vector<Field> head;
			head.push_back(select<config::line::normal::main::Use>("Content",
				[](auto& c) -> auto& { return c.line.normal.main.use; },
				{{"Syllable", config::line::normal::main::Use::Syllable}, {"Plain", config::line::normal::main::Use::Plain}}));

			std::vector<Field> floating;
			floating.push_back(toggle("Enabled", [animation](auto& c) -> auto& { return animation(c).floating.enabled; }));
			floating.push_back(number("From", [animation](auto& c) -> auto& { return animation(c).floating.from; }, -50.0, 50.0, 0.5));
			floating.push_back(number("To", [animation](auto& c) -> auto& { return animation(c).floating.to; }, -50.0, 50.0, 0.5));

			std::vector<Field> mask;
			mask.push_back(toggle("Enabled", [animation](auto& c) -> auto& { return animation(c).mask.enabled; }));
			mask.push_back(number("Feather", [animation](auto& c) -> auto& { return animation(c).mask.feather.normal; }, 0.0, 2.0, 0.05));
			mask.push_back(number("First word", [animation](auto& c) -> auto& { return animation(c).mask.feather.first; }, 0.0, 5.0, 0.05));
			mask.push_back(number("Last word", [animation](auto& c) -> auto& { return animation(c).mask.feather.last; }, 0.0, 5.0, 0.05));

			std::vector<Field> stress;
			stress.push_back(toggle("Enabled", [emphasize](auto& c) -> auto& { return emphasize(c).enabled; }));
			stress.push_back(number("Min duration", [emphasize](auto& c) -> auto& { return emphasize(c).minDuration; }, 0.0, 5000.0, 50.0));
			stress.push_back(number("Wind-down rate", [emphasize](auto& c) -> auto& { return emphasize(c).disablePlaybackRate; }, 1.0, 20.0, 0.5));

			std::vector<Field> stressMain;
			stressMain.push_back(toggle("Enabled", [emphasize](auto& c) -> auto& { return emphasize(c).effects.main.enabled; }));
			stressMain.push_back(number("Scale", [emphasize](auto& c) -> auto& { return emphasize(c).effects.main.scale; }, 0.0, 1.0, 0.01));
			stressMain.push_back(number("Offset X", [emphasize](auto& c) -> auto& { return emphasize(c).effects.main.offsetHorizontal; }, 0.0, 20.0, 0.1));
			stressMain.push_back(number("Offset Y", [emphasize](auto& c) -> auto& { return emphasize(c).effects.main.offsetVertical; }, 0.0, 20.0, 0.1));
			stressMain.push_back(text("Rise easing", [emphasize](auto& c) -> auto& { return emphasize(c).effects.main.easingRise; }));
			stressMain.push_back(text("Fall easing", [emphasize](auto& c) -> auto& { return emphasize(c).effects.main.easingFall; }));

			std::vector<Field> glow;
			glow.push_back(toggle("Enabled", [emphasize](auto& c) -> auto& { return emphasize(c).effects.glow.enabled; }));
			glow.push_back(text("Color", [emphasize](auto& c) -> auto& { return emphasize(c).effects.glow.color; }));
			glow.push_back(number("Max radius", [emphasize](auto& c) -> auto& { return emphasize(c).effects.glow.maxRadius; }, 0.0, 50.0, 0.5));
			glow.push_back(number("Max alpha", [emphasize](auto& c) -> auto& { return emphasize(c).effects.glow.maxAlpha; }, 0.0, 1.0, 0.05));
			glow.push_back(text("Rise easing", [emphasize](auto& c) -> auto& { return emphasize(c).effects.glow.easingRise; }));
			glow.push_back(text("Fall easing", [emphasize](auto& c) -> auto& { return emphasize(c).effects.glow.easingFall; }));

			std::vector<Field> bounce;
			bounce.push_back(toggle("Enabled", [emphasize](auto& c) -> auto& { return emphasize(c).effects.floating.enabled; }));
			bounce.push_back(number("Amplitude", [emphasize](auto& c) -> auto& { return emphasize(c).effects.floating.amplitude; }, 0.0, 20.0, 0.5));
			bounce.push_back(number("Duration scale", [emphasize](auto& c) -> auto& { return emphasize(c).effects.floating.duration.scale; }, 0.0, 5.0, 0.1));
			bounce.push_back(number("Lead", [emphasize](auto& c) -> auto& { return emphasize(c).effects.floating.duration.lead; }, 0.0, 2000.0, 50.0));
			bounce.push_back(text("Easing", [emphasize](auto& c) -> auto& { return emphasize(c).effects.floating.easing; }));

			std::vector<Group> groups;
			groups.push_back(Group{"", std::move(head)});
			groups.push_back(fontGroup(syllable));
			append(groups, styleGroups<true>(syllable));
			groups.push_back(Group{"Word float", std::move(floating)});
			groups.push_back(Group{"Word mask", std::move(mask)});
			groups.push_back(Group{"Emphasize", std::move(stress)});
			groups.push_back(Group{"Emphasize main", std::move(stressMain)});
			groups.push_back(Group{"Emphasize glow", std::move(glow)});
			groups.push_back(Group{"Emphasize float", std::move(bounce)});
			return Section{"Normal main", std::move(groups), {}};
		}

		/**
		 * Builds one annotation row section, shared by the roman and translate rows.
		 */
		template <typename At>
		Section annotationRow(std::string title, At at) {
			std::vector<Field> head;
			head.push_back(toggle("Visible", [at](auto& c) -> auto& { return at(c).visible; }));
			head.push_back(choice("Language", [at](auto& c) -> auto& { return at(c).language; }, languages(true)));

			std::vector<Group> groups;
			groups.push_back(Group{"", std::move(head)});
			groups.push_back(fontGroup(at));
			append(groups, styleGroups<true>(at));
			return Section{std::move(title), std::move(groups), {}};
		}

		/**
		 * Builds the line section: the normal line's base row, main content and annotations, then the interlude.
		 */
		Section lineSection() {
			const auto base       = [](auto& c) -> auto& { return c.line.normal.base; };
			const auto annotation = [](auto& c) -> auto& { return c.line.normal.annotation.base; };
			const auto interlude  = [](auto& c) -> auto& { return c.line.interlude; };

			std::vector<Group> baseGroups;
			baseGroups.push_back(Group{"", {choice("Language", [base](auto& c) -> auto& { return base(c).language; }, languages(false))}});
			baseGroups.push_back(fontGroup(base));
			append(baseGroups, styleGroups<true>(base));

			std::vector<Group> annotationGroups;
			annotationGroups.push_back(Group{"", {toggle("Visible", [](auto& c) -> auto& { return c.line.normal.annotation.visible; })}});
			annotationGroups.push_back(fontGroup(annotation));
			append(annotationGroups, styleGroups<true>(annotation));

			std::vector<Group> interludeGroups;
			interludeGroups.push_back(Group{"", {text("Size", [interlude](auto& c) -> auto& { return interlude(c).size; })}});
			append(interludeGroups, styleGroups<false>(interlude));

			std::vector<Section> children;
			children.push_back(Section{"Normal base", std::move(baseGroups), {}});
			children.push_back(mainSection());
			children.push_back(Section{"Normal annotation",
				std::move(annotationGroups),
				{annotationRow("Roman", [](auto& c) -> auto& { return c.line.normal.annotation.roman; }),
					annotationRow("Translate", [](auto& c) -> auto& { return c.line.normal.annotation.translate; })}});
			children.push_back(Section{"Interlude", std::move(interludeGroups), {}});
			return Section{"Line", {}, std::move(children)};
		}

		/**
		 * Builds the whole editor tree once, so the text fields keep their scratch buffers between frames.
		 */
		const std::vector<Section>& sections() {
			static const std::vector<Section> tree = [] {
				std::vector<Section> result;
				result.push_back(containerSection());
				for (Section& section : layoutSections()) {
					result.push_back(std::move(section));
				}
				result.push_back(scrollSection());
				result.push_back(lineSection());
				return result;
			}();
			return tree;
		}

		/**
		 * Draws one run of fields as label and control rows.
		 */
		void drawRows(const std::vector<Field>& fields, std::size_t from, const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
			const theme::Metrics& metrics = theme::metrics();

			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * metrics.scale, 4.0f * metrics.scale));
			for (std::size_t i = from; i < fields.size(); ++i) {
				// Labels repeat across groups, so the row index is what keeps the controls apart in ImGui's id stack.
				ImGui::PushID(static_cast<int>(i));
				widgets::field(fields[i].label.c_str());
				fields[i].draw(current, overrides, edit);
				ImGui::PopID();
			}
			ImGui::PopStyleVar();
		}

		/**
		 * Draws one group: an untitled run sits straight on the section body, a titled one inside its own card.
		 */
		void drawGroup(const Group& group, const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
			if (group.title.empty()) {
				drawRows(group.fields, 0, current, overrides, edit);
				return;
			}

			widgets::beginCard(group.title.c_str());

			std::size_t first = 0;
			bool        open  = true;
			if (!group.fields.empty() && group.fields[0].master) {
				// A leading switch owns the card: it moves up into the title row and the rest folds with it.
				open  = group.fields[0].master(group.title.c_str(), current, overrides, edit);
				first = 1;
			} else if (group.collapsible) {
				open = widgets::groupHeader(group.title.c_str());
			} else {
				widgets::groupTitle(group.title.c_str());
			}

			if (open && first < group.fields.size()) {
				drawRows(group.fields, first, current, overrides, edit);
			}
			widgets::endCard();
		}

		/**
		 * Draws one accordion section and everything nested under it.
		 */
		void drawSection(const Section& section, int level, const config::Root& current, config::Root& overrides, SettingsEdit& edit) {
			const theme::Metrics& metrics = theme::metrics();
			if (!widgets::sectionHeader(section.title.c_str(), level)) {
				return;
			}

			if (!section.groups.empty()) {
				// The body steps in with the head it belongs to, and keeps the frame's padding on its right.
				const float indent = level == 0 ? metrics.framePadding : (level == 1 ? 22.0f * metrics.scale : 26.0f * metrics.scale);
				widgets::beginInset(indent, metrics.framePadding);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 12.0f * metrics.scale));
				ImGui::Dummy(ImVec2(0.0f, 0.0f));
				for (std::size_t i = 0; i < section.groups.size(); ++i) {
					ImGui::PushID(static_cast<int>(i));
					drawGroup(section.groups[i], current, overrides, edit);
					ImGui::PopID();
				}
				ImGui::PopStyleVar();
				widgets::endInset();
				ImGui::Dummy(ImVec2(0.0f, 12.0f * metrics.scale));
			}

			for (std::size_t i = 0; i < section.children.size(); ++i) {
				widgets::rule();
				// Child indices share a scope with the group indices above, so they are offset out of the way.
				ImGui::PushID(static_cast<int>(i) + 1000);
				drawSection(section.children[i], level + 1, current, overrides, edit);
				ImGui::PopID();
			}
		}
	} // namespace

	SettingsEdit drawSettings(const config::Root& current, config::Root& overrides) {
		SettingsEdit edit;

		const std::vector<Section>& tree = sections();
		for (std::size_t i = 0; i < tree.size(); ++i) {
			if (i > 0) {
				widgets::rule();
			}
			ImGui::PushID(static_cast<int>(i));
			drawSection(tree[i], 0, current, overrides, edit);
			ImGui::PopID();
		}
		return edit;
	}
} // namespace example
