/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/lang_keys.h"
#include "settings/sections/settings_custom.h"

#include "custom_features/custom_settings.hpp"
#include "custom_features/session_wipe.hpp"
#include "custom_features/in_game_overlay.hpp"
#include "custom_features/ws_proxy_manager.hpp"
#include "custom_features/memory_optimizer.hpp"
#include "boxes/connection_box.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "settings/settings_common_session.h"
#include "settings/sections/settings_main.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "main/main_session.h"
#include "main/main_domain.h"
#include "storage/storage_domain.h"
#include "storage/localstorage.h"
#include "settings/sections/settings_local_passcode.h"
#include "window/themes/window_theme.h"
#include "ui/vertical_list.h"
#include "ui/widgets/buttons.h"
#include "ui/widgets/checkbox.h"
#include "ui/widgets/continuous_sliders.h"
#include "ui/widgets/labels.h"
#include "ui/layers/generic_box.h"
#include "ui/boxes/confirm_box.h"
#include "ui/wrap/vertical_layout.h"
#include <QtGui/QMouseEvent>
#include "ui/search_field_controller.h"
#include "ui/painter.h"
#include "window/main_window.h"
#include "styles/style_info.h"
#include "window/window_session_controller.h"
#include "styles/style_basic.h"
#include "styles/style_boxes.h"
#include "styles/style_menu_icons.h"
#include "styles/style_settings.h"
#include "styles/style_layers.h"

namespace Settings {
namespace {

using namespace Builder;

class CustomSection : public Section<CustomSection> {
public:
	CustomSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

class CustomPrivacySection : public Section<CustomPrivacySection> {
public:
	CustomPrivacySection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

class CustomMessagesSection : public Section<CustomMessagesSection> {
public:
	CustomMessagesSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

class CustomProxySection : public Section<CustomProxySection> {
public:
	CustomProxySection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

class CustomInterfaceSection : public Section<CustomInterfaceSection> {
public:
	CustomInterfaceSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

class CustomPanicSection : public Section<CustomPanicSection> {
public:
	CustomPanicSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

class CustomAboutSection : public Section<CustomAboutSection> {
public:
	CustomAboutSection(
		QWidget *parent,
		not_null<Window::SessionController*> controller);

	[[nodiscard]] rpl::producer<QString> title() override;

private:
	void setupContent();

};

// 1. Приватность и «Невидимка»
const auto kPrivacyMeta = BuildHelper({
	.id = CustomPrivacySection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_privacy,
	.icon = &st::menuIconStealth,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Ссылки и трекинг"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/clean_urls"_q,
		.title = rpl::single(u"Очистка UTM-меток и трекеров"_q),
		.checked = CustomFeatures::GetConfig().cleanTrackingUrls,
		.keywords = { u"clean"_q, u"url"_q, u"utm"_q, u"tracking"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().cleanTrackingUrls = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Автоматически вырезает метки отслеживания (utm_*, fbclid, gclid, yclid) из ссылок при копировании."_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/direct_links"_q,
		.title = rpl::single(u"Прямой переход по внешним ссылкам"_q),
		.checked = CustomFeatures::GetConfig().directExternalLinks,
		.keywords = { u"links"_q, u"external"_q, u"direct"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().directExternalLinks = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Открывает внешние веб-ссылки в браузере сразу без всплывающего окна подтверждения перехода."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Приватность историй"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/hide_stories"_q,
		.title = rpl::single(u"Скрыть блок историй (Stories)"_q),
		.checked = CustomFeatures::GetConfig().hideStoriesBar,
		.keywords = { u"stories"_q, u"hide"_q, u"privacy"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().hideStoriesBar = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Скрывает плашку историй сверху над списком чатов для чистоты интерфейса."_q));
});

CustomPrivacySection::CustomPrivacySection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomPrivacySection::title() {
	return rpl::single(u"Приватность и «Невидимка»"_q);
}

void CustomPrivacySection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kPrivacyMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 2. Стикеры, Медиа и Загрузки
const auto kMessagesMeta = BuildHelper({
	.id = CustomMessagesSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_chat_settings,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Стикеры и эмодзи"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/unlimited_stickers"_q,
		.title = rpl::single(u"Расширенный список недавних стикеров (до 300)"_q),
		.checked = CustomFeatures::GetConfig().unlimitedRecentStickers,
		.keywords = { u"stickers"_q, u"recent"_q, u"unlimited"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().unlimitedRecentStickers = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Увеличивает вместимость списка недавно использованных стикеров с 20 до 300 штук."_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/unlimited_all_stickers"_q,
		.title = rpl::single(u"Безлимитные стикерпаки в панели"_q),
		.checked = CustomFeatures::GetConfig().unlimitedStickers,
		.keywords = { u"stickers"_q, u"packs"_q, u"archived"_q, u"unlimited"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().unlimitedStickers = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Отображает все ваши наборы (включая архивированные) в панели выбора без лимита в 200 наборов."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Умные загрузки"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/downloads_router"_q,
		.title = rpl::single(u"Сортировка загрузок по категориям"_q),
		.checked = CustomFeatures::GetConfig().enableDownloadsRouter,
		.keywords = { u"downloads"_q, u"router"_q, u"folders"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableDownloadsRouter = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Автоматически раскладывает сохраняемые файлы по папкам: Фото, Видео, Музыка, Документы."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Оптимизация памяти (RAM)"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/trim_memory_on_minimize"_q,
		.title = rpl::single(u"Сбрасывать память при сворачивании в трей"_q),
		.checked = CustomFeatures::GetConfig().trimMemoryOnMinimize,
		.keywords = { u"ram"_q, u"memory"_q, u"minimize"_q, u"optimize"_q, u"tray"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().trimMemoryOnMinimize = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Автоматически выгружает неактивные графические кэши и сбрасывает рабочий набор памяти Windows при сворачивании окна или уходе в трей."_q));

	builder.addButton({
		.title = rpl::single(u"Очистить память RAM сейчас"_q),
		.icon = &st::menuIconDelete,
		.onClick = [=] {
			CustomFeatures::MemoryOptimizer::TrimWorkingSet();
			builder.controller()->showToast(u"Память успешно оптимизирована!"_q);
		},
		.keywords = { u"ram"_q, u"memory"_q, u"clear"_q, u"clean"_q },
	});
	builder.addDividerText(rpl::single(u"Мгновенно очищает кэш растровых изображений и возвращает свободную память операционной системе."_q));
});

CustomMessagesSection::CustomMessagesSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomMessagesSection::title() {
	return rpl::single(u"Стикеры, Медиа и Загрузки"_q);
}

void CustomMessagesSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMessagesMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 3. Обход блокировок и Сеть
const auto kProxyMeta = BuildHelper({
	.id = CustomProxySection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_advanced,
	.icon = &st::menuIconNetwork,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Встроенный MTProto Прокси (Обход блокировок)"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/ws_proxy_enable"_q,
		.title = rpl::single(u"Включить tg-ws-proxy (WebSocket / Cloudflare)"_q),
		.checked = CustomFeatures::GetConfig().enableWsProxy,
		.keywords = { u"proxy"_q, u"tg-ws-proxy"_q, u"bypass"_q, u"cloudflare"_q, u"mtproto"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableWsProxy = checked;
			CustomFeatures::GetConfig().save();
			if (checked) {
				CustomFeatures::WsProxyManager::Instance().startProxy();
			} else {
				CustomFeatures::WsProxyManager::Instance().stopProxy();
			}
		}, check->lifetime());
	}

	builder.addDividerText(rpl::single(
		u"Встроенный обход замедления и блокировок Telegram на базе tg-ws-proxy (Flowseal). "
		u"Маскирует трафик под HTTPS/WebSocket через инфраструктуру Cloudflare CDN. "
		u"Работает полностью в фоне и автоматически подключается в клиенте."_q));

	builder.addButton({
		.title = rpl::single(u"Управление tg-ws-proxy (Статус / Перезапуск)"_q),
		.icon = &st::menuIconNetwork,
		.onClick = [=] {
			if (!CustomFeatures::WsProxyManager::Instance().hasExecutable()) {
				builder.controller()->show(Ui::MakeConfirmBox({
					.text = u"Исполняемый файл tg-ws-proxy.exe не найден рядом с клиентом.\n\nСкачать его автоматически из официального репозитория Flowseal/tg-ws-proxy?"_q,
					.confirmed = [=] {
						CustomFeatures::WsProxyManager::Instance().downloadProxyAsync([=](bool ok, QString err) {
							if (ok) {
								if (CustomFeatures::GetConfig().enableWsProxy) {
									CustomFeatures::WsProxyManager::Instance().startProxy();
								}
							}
						});
					},
					.confirmText = u"Скачать"_q,
					.cancelText = tr::lng_cancel(),
				}));
			} else {
				builder.controller()->show(Box([=](not_null<Ui::GenericBox*> box) {
					box->setTitle(rpl::single(u"Статус tg-ws-proxy"_q));
					const auto layout = box->verticalLayout();
					const auto exe = CustomFeatures::WsProxyManager::Instance().findProxyExecutable();
					Ui::AddDividerText(layout, rpl::single(
						u"Путь к файлу:\n"_q + (exe.isEmpty() ? u"Не найден"_q : exe)));
					Ui::AddDividerText(layout, rpl::single(
						u"Порт: "_q + QString::number(CustomFeatures::GetConfig().wsProxyPort) +
						u"\nАдрес: 127.0.0.1\nСостояние: "_q +
						CustomFeatures::WsProxyManager::Instance().statusString()));

					box->addButton(rpl::single(u"Перезапустить"_q), [=] {
						box->closeBox();
						CustomFeatures::WsProxyManager::Instance().restartProxy();
					});
					box->addButton(tr::lng_close(), [=] { box->closeBox(); });
				}));
			}
		},
		.keywords = { u"proxy"_q, u"status"_q, u"ws"_q },
	});

	builder.addButton({
		.title = rpl::single(u"Список прокси Telegram"_q),
		.icon = &st::menuIconNetwork,
		.onClick = [=] {
			ProxiesBoxController::Show(builder.controller());
		},
		.keywords = { u"proxy"_q, u"settings"_q, u"list"_q },
	});
});

CustomProxySection::CustomProxySection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomProxySection::title() {
	return rpl::single(u"Обход блокировок и Сеть"_q);
}

void CustomProxySection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kProxyMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 4. Интерфейс, Гейминг и Стиль
const auto kInterfaceMeta = BuildHelper({
	.id = CustomInterfaceSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_features,
	.icon = &st::menuIconPalette,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Очистка интерфейса"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/hide_ads"_q,
		.title = rpl::single(u"Скрыть рекламу в каналах и ботах"_q),
		.checked = CustomFeatures::GetConfig().hideSponsoredAds,
		.keywords = { u"ads"_q, u"hide"_q, u"sponsored"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().hideSponsoredAds = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	if (const auto check = builder.addCheckbox({
		.id = u"custom/hide_premium_promos"_q,
		.title = rpl::single(u"Скрыть промо Premium подписки"_q),
		.checked = CustomFeatures::GetConfig().hidePremiumPromos,
		.keywords = { u"premium"_q, u"hide"_q, u"promo"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().hidePremiumPromos = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	if (const auto check = builder.addCheckbox({
		.id = u"custom/windows_accent"_q,
		.title = rpl::single(u"Цвет акцента из Windows (под обои / Wallpaper Engine)"_q),
		.checked = CustomFeatures::GetConfig().syncWindowsAccentColor,
		.keywords = { u"accent"_q, u"color"_q, u"windows"_q, u"wallpaper"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().syncWindowsAccentColor = checked;
			CustomFeatures::GetConfig().save();
			Core::App().settings().setSystemAccentColorEnabled(checked);
			Local::writeSettings();
			const auto path = Window::Theme::Background()->themeObject().pathAbsolute;
			if (!path.isEmpty()) {
				Window::Theme::Apply(path);
				Window::Theme::KeepApplied();
			}
		}, check->lifetime());
	}

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Гейминг и оверлей"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/game_overlay"_q,
		.title = rpl::single(u"Игровой оверлей (Shift + ~)"_q),
		.checked = CustomFeatures::GetConfig().enableInGameOverlay,
		.keywords = { u"overlay"_q, u"game"_q, u"shift"_q, u"tilda"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableInGameOverlay = checked;
			CustomFeatures::GetConfig().save();
			CustomFeatures::InGameOverlayManager::Instance().updateState();
		}, check->lifetime());
	}

	builder.addButton({
		.title = rpl::single(u"Выбрать игры для оверлея"_q),
		.icon = &st::menuIconDevices,
		.onClick = [=] {
			builder.controller()->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(rpl::single(u"Игры для оверлея"_q));

				const auto layout = box->verticalLayout();

				Ui::AddDividerText(layout, rpl::single(u"Оверлей открывается по горячим клавишам Shift + ~ или Shift + F11 поверх игр:"_q));

				const auto allCheck = layout->add(object_ptr<Ui::Checkbox>(
					box,
					u"Работать поверх всех игр и приложений"_q,
					CustomFeatures::GetConfig().overlayAllGames,
					st::defaultCheckbox));
				allCheck->checkedChanges() | rpl::on_next([=](bool checked) {
					CustomFeatures::GetConfig().overlayAllGames = checked;
					CustomFeatures::GetConfig().save();
				}, box->lifetime());

				Ui::AddSkip(layout);
				Ui::AddSubsectionTitle(layout, rpl::single(u"Сохраненные игры (нажмите для удаления)"_q));

				if (CustomFeatures::GetConfig().overlayAllowedGames.isEmpty()) {
					Ui::AddDividerText(layout, rpl::single(u"Список пуст (оверлей работает везде)"_q));
				} else {
					for (const auto &game : CustomFeatures::GetConfig().overlayAllowedGames) {
						const auto btn = layout->add(object_ptr<Ui::SettingsButton>(
							box,
							rpl::single(u"🎮 "_q + game + u"   [✕ Удалить]"_q),
							st::settingsButtonNoIcon));
						btn->setClickedCallback([=] {
							CustomFeatures::GetConfig().overlayAllowedGames.removeAll(game);
							CustomFeatures::GetConfig().save();
							box->closeBox();
						});
					}
				}

				Ui::AddSkip(layout);
				Ui::AddSubsectionTitle(layout, rpl::single(u"Добавить из запущенных игр"_q));

				const auto running = CustomFeatures::GetRunningUserApps();
				if (running.isEmpty()) {
					Ui::AddDividerText(layout, rpl::single(u"Запущенных пользовательских окон не найдено"_q));
				} else {
					for (const auto &app : running) {
						const bool already = CustomFeatures::GetConfig().overlayAllowedGames.contains(app.exeName, Qt::CaseInsensitive);
						const auto itemTitle = (already ? u"✓ "_q : u"+ "_q) + app.windowTitle + u" ("_q + app.exeName + u")"_q;
						const auto btn = layout->add(object_ptr<Ui::SettingsButton>(
							box,
							rpl::single(itemTitle),
							st::settingsButtonNoIcon));
						btn->setClickedCallback([=] {
							if (!CustomFeatures::GetConfig().overlayAllowedGames.contains(app.exeName, Qt::CaseInsensitive)) {
								CustomFeatures::GetConfig().overlayAllowedGames.append(app.exeName);
								CustomFeatures::GetConfig().save();
								box->closeBox();
							}
						});
					}
				}

				box->addButton(tr::lng_close(), [=] { box->closeBox(); });
			}));
		},
		.keywords = { u"game"_q, u"process"_q, u"list"_q, u"overlay"_q },
	});

	if (const auto check = builder.addCheckbox({
		.id = u"custom/game_status"_q,
		.title = rpl::single(u"Авто-статус запущенных игр"_q),
		.checked = CustomFeatures::GetConfig().enableGameStatus,
		.keywords = { u"game"_q, u"status"_q, u"activity"_q, u"cs2"_q, u"dota"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableGameStatus = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Скругление углов элементов"_q));

	// Скругление углов сообщений
	builder.addSubsectionTitle(rpl::single(u"Скругление углов сообщений"_q));
	builder.add([](const WidgetContext &ctx) {
		auto result = MakeSliderWithLabel(
			ctx.container.get(),
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("24 px"),
			false);

		const auto slider = result.slider;
		const auto label = result.label;

		const int currentRadius = CustomFeatures::GetConfig().messageBorderRadius;
		slider->setPseudoDiscrete(
			25,
			[](int val) { return val; },
			currentRadius,
			[=](int val) {
				CustomFeatures::GetConfig().messageBorderRadius = val;
				CustomFeatures::GetConfig().save();
				label->setText(QString::number(val) + u" px"_q);
			});

		label->setText(QString::number(currentRadius) + u" px"_q);

		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result.widget),
			.margin = st::settingsScalePadding,
		};
	});

	builder.addSkip(st::settingsCheckboxesSkip);

	// Скругление углов чатов в списке
	builder.addSubsectionTitle(rpl::single(u"Скругление углов чатов в списке"_q));
	builder.add([](const WidgetContext &ctx) {
		auto result = MakeSliderWithLabel(
			ctx.container.get(),
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("24 px"),
			false);

		const auto slider = result.slider;
		const auto label = result.label;

		const int currentRadius = CustomFeatures::GetConfig().chatBorderRadius;
		slider->setPseudoDiscrete(
			25,
			[](int val) { return val; },
			currentRadius,
			[=](int val) {
				CustomFeatures::GetConfig().chatBorderRadius = val;
				CustomFeatures::GetConfig().save();
				label->setText(QString::number(val) + u" px"_q);
			});

		label->setText(QString::number(currentRadius) + u" px"_q);

		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result.widget),
			.margin = st::settingsScalePadding,
		};
	});

	builder.addSkip(st::settingsCheckboxesSkip);

	if (const auto check = builder.addCheckbox({
		.id = u"custom/soft_shadows"_q,
		.title = rpl::single(u"Мягкие тени под элементами"_q),
		.checked = CustomFeatures::GetConfig().enableSoftShadows,
		.keywords = { u"shadows"_q, u"ui"_q, u"design"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableSoftShadows = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Плавная прокрутка (Fluid Scroll)"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/smooth_scrolling"_q,
		.title = rpl::single(u"Плавная прокрутка"_q),
		.checked = CustomFeatures::GetConfig().smoothScrolling,
		.keywords = { u"smooth"_q, u"scroll"_q, u"animation"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().smoothScrolling = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	if (const auto check = builder.addCheckbox({
		.id = u"custom/motion_blur"_q,
		.title = rpl::single(u"Имитация высокой герцовки (Fluid Scroll)"_q),
		.checked = CustomFeatures::GetConfig().enableMotionBlur,
		.keywords = { u"motion"_q, u"blur"_q, u"smooth"_q, u"animation"_q, u"hz"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableMotionBlur = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addSubsectionTitle(rpl::single(u"Плавность и инерция прокрутки"_q));
	builder.add([](const WidgetContext &ctx) {
		auto result = MakeSliderWithLabel(
			ctx.container.get(),
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("10 / 10"),
			false);

		const auto slider = result.slider;
		const auto label = result.label;

		const int currentIntensity = CustomFeatures::GetConfig().motionBlurIntensity;
		slider->setPseudoDiscrete(
			10,
			[](int val) { return val + 1; },
			currentIntensity,
			[=](int val) {
				CustomFeatures::GetConfig().motionBlurIntensity = val;
				CustomFeatures::GetConfig().save();
				label->setText(QString::number(val) + u" / 10"_q);
			});

		label->setText(QString::number(currentIntensity) + u" / 10"_q);

		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result.widget),
			.margin = st::settingsScalePadding,
		};
	});

	builder.addSkip(st::settingsCheckboxesSkip);
	builder.addDividerText(rpl::single(u"Адаптивная инерция прокрутки колесиком мыши (эффект 120-240 Гц). Устраняет рывки и сохраняет идеальную чёткость текста без просадок FPS."_q));
	builder.addDividerText(rpl::single(u"Перезапустите Pawgram для применения изменений скругления углов."_q));
});

CustomInterfaceSection::CustomInterfaceSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomInterfaceSection::title() {
	return rpl::single(u"Интерфейс, Гейминг и Стиль"_q);
}

void CustomInterfaceSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kInterfaceMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 5. Безопасность и Экстренная очистка
const auto kPanicMeta = BuildHelper({
	.id = CustomPanicSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_privacy,
	.icon = &st::menuIconDelete,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Защита и блокировка"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/auto_lock"_q,
		.title = rpl::single(u"Блокировка клиента по Win + L"_q),
		.checked = CustomFeatures::GetConfig().autoLockOnWindowsLock,
		.keywords = { u"lock"_q, u"security"_q, u"win+l"_q, u"passcode"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().autoLockOnWindowsLock = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Автоматически блокирует клиент код-паролем при блокировке экрана Windows."_q));

	builder.addButton({
		.title = rpl::single(u"Настроить код-пароль блокировки"_q),
		.icon = &st::menuIconLock,
		.onClick = [=] {
			if (builder.session()->domain().local().hasLocalPasscode()) {
				builder.showOther()(LocalPasscodeCheckId());
			} else {
				builder.showOther()(LocalPasscodeCreateId());
			}
		},
		.keywords = { u"passcode"_q, u"pin"_q, u"lock"_q, u"password"_q },
	});

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Экстренная очистка (Panic Wipe)"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/auto_wipe"_q,
		.title = rpl::single(u"Полная очистка при выходе из аккаунта"_q),
		.checked = CustomFeatures::GetConfig().autoWipeOnLogout,
		.keywords = { u"wipe"_q, u"logout"_q, u"tdata"_q, u"privacy"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().autoWipeOnLogout = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(u"Автоматически стирает tdata и кэш при обычном выходе (Logout), предотвращая сохранение следов."_q));

	builder.addButton({
		.title = rpl::single(u"Экстренная очистка (Wipe Data & Logout)"_q),
		.icon = &st::menuIconLeaveAttention,
		.onClick = [=] {
			builder.controller()->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(rpl::single(u"Экстренная очистка данных"_q));

				const auto layout = box->verticalLayout();

				Ui::AddDividerText(
					layout,
					rpl::single(u"Внимание! Это действие завершит сеанс на серверах Telegram и безвозвратно удалит все локальные данные с этого компьютера:"_q));

				Ui::AddDividerText(
					layout,
					rpl::single(u"• Папку аккаунта и ключи сессий (tdata)\n• Историю сообщений, кэш и базы данных\n• Все файлы логов (log.txt, DebugLogs)\n• Данные tg-ws-proxy\n• Локальные настройки клиента"_q));

				Ui::AddDividerText(
					layout,
					rpl::single(u"Идеально подходит для работы на чужом или общественном ПК: никаких следов вашего пребывания не останется."_q));

				box->addButton(
					rpl::single(u"Очистить и закрыть"_q),
					[=] {
						box->closeBox();
						CustomFeatures::WipeSessionAndExit(false);
					},
					st::attentionBoxButton);

				box->addButton(
					rpl::single(u"Очистить и перезапустить"_q),
					[=] {
						box->closeBox();
						CustomFeatures::WipeSessionAndExit(true);
					});

				box->addButton(tr::lng_cancel(), [=] {
					box->closeBox();
				});
			}));
		},
		.keywords = { u"wipe"_q, u"logout"_q, u"clean"_q, u"privacy"_q, u"tdata"_q },
	});
});

CustomPanicSection::CustomPanicSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomPanicSection::title() {
	return rpl::single(u"Безопасность и Экстренная очистка"_q);
}

void CustomPanicSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kPanicMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 6. О клиенте и Оптимизация
const auto kAboutMeta = BuildHelper({
	.id = CustomAboutSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_info,
	.icon = &st::menuIconCustomize,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"О проекте Pawgram"_q));
	builder.addDividerText(rpl::single(
		u"Pawgram Desktop — модифицированный клиент Telegram с открытым исходным кодом, "
		"созданный для максимальной конфиденциальности, скорости работы и удобства.\n\n"
		"Особенности сборки:\n"
		"• Защита от трекеров и скрытие статусов (Ghost Mode)\n"
		"• Встроенный туннель tg-ws-proxy через Cloudflare\n"
		"• Игровой оверлей и оптимизация интерфейса\n"
		"• Экстренная очистка данных (Panic Wipe)"_q));

	builder.addSubsectionTitle(rpl::single(u"Система и оптимизация"_q));

	builder.addButton({
		.title = rpl::single(u"Очистить оперативную память (RAM Optimizer)"_q),
		.icon = &st::menuIconManage,
		.onClick = [=] {
			CustomFeatures::MemoryOptimizer::TrimWorkingSet();
			builder.controller()->showToast(u"Оперативная память успешно оптимизирована!"_q);
		},
		.keywords = { u"ram"_q, u"memory"_q, u"optimize"_q, u"память"_q, u"очистка"_q },
	});
	builder.addDividerText(rpl::single(u"Сбрасывает графический кэш Qt и выгружает неактивные страницы из рабочей памяти процесса."_q));

	builder.addButton({
		.title = rpl::single(u"Сбросить настройки мода по умолчанию"_q),
		.icon = &st::menuIconDelete,
		.onClick = [=] {
			builder.controller()->show(Ui::MakeConfirmBox({
				.text = u"Вы действительно хотите сбросить все кастомные настройки Pawgram к значениям по умолчанию?"_q,
				.confirmed = [=](Fn<void()> close) {
					close();
					CustomFeatures::GetConfig() = CustomFeatures::ClientConfig();
					CustomFeatures::GetConfig().save();
					builder.controller()->showToast(u"Настройки сброшены по умолчанию."_q);
				},
				.confirmText = u"Сбросить"_q,
			}));
		},
		.keywords = { u"reset"_q, u"default"_q, u"сброс"_q },
	});

	builder.addDivider();
	builder.addDividerText(rpl::single(
		u"Версия сборки: Pawgram Desktop v7.0.9 (Release)\n"
		"Базируется на Telegram Desktop 5.x\n"
		"Разработчик: Heck"_q));
});

CustomAboutSection::CustomAboutSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomAboutSection::title() {
	return rpl::single(u"О клиенте"_q);
}

void CustomAboutSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kAboutMeta.build);
	Ui::ResizeFitChild(this, content);
}

[[nodiscard]] std::vector<Type> AllCustomSections() {
	return {
		CustomPrivacySection::Id(),
		CustomMessagesSection::Id(),
		CustomProxySection::Id(),
		CustomInterfaceSection::Id(),
		CustomPanicSection::Id(),
		CustomAboutSection::Id(),
	};
}

void FillSearchResults(
		not_null<Ui::VerticalLayout*> container,
		not_null<Window::SessionController*> controller,
		Fn<void(Type)> showOther,
		const std::vector<Type> &sections,
		const QString &query) {
	container->clear();
	const auto trimmed = query.trimmed().toLower();
	if (trimmed.isEmpty()) {
		container->resizeToWidth(container->width());
		return;
	}
	const auto &registry = Builder::SearchRegistry::Instance();
	int matchesCount = 0;
	for (const auto &entry : registry.collectAll(&controller->session())) {
		if (entry.id.isEmpty() || !ranges::contains(sections, entry.section)) {
			continue;
		}
		bool matches = entry.title.toLower().contains(trimmed);
		if (!matches) {
			for (const auto &kw : entry.keywords) {
				if (kw.toLower().contains(trimmed)) {
					matches = true;
					break;
				}
			}
		}
		if (matches) {
			matchesCount++;
			auto button = container->add(object_ptr<Ui::SettingsButton>(
				container,
				rpl::single(entry.title),
				st::settingsButtonNoIcon));
			button->setClickedCallback([=] {
				crl::on_main(controller, [=] {
					controller->setHighlightControlId(entry.id);
					showOther(entry.section);
				});
			});
			if (matchesCount >= 25) {
				break;
			}
		}
	}
	if (matchesCount == 0) {
		container->add(
			object_ptr<Ui::FlatLabel>(
				container,
				tr::lng_search_tab_no_results(),
				st::settingsSearchNoResults),
			st::settingsSearchNoResultsPadding);
	}
	container->resizeToWidth(container->width());
}

void AddHeader(SectionBuilder &builder) {
	builder.addControl({
		.factory = [=](not_null<Ui::VerticalLayout*> container) {
			auto result = object_ptr<Ui::VerticalLayout>(container);
			const auto raw = result.data();
			constexpr auto size = 64;
			const auto logo = raw->add(
				object_ptr<Ui::FixedHeightWidget>(raw, size),
				QMargins(0, 16, 0, 0));
			logo->paintRequest(
			) | rpl::on_next([=] {
				auto p = QPainter(logo);
				auto hq = PainterHighQualityEnabler(p);
				const auto left = (logo->width() - size) / 2;
				p.drawImage(
					QRect(left, 0, size, size),
					Window::Logo());
			}, logo->lifetime());
			raw->add(
				object_ptr<Ui::FlatLabel>(
					raw,
					rpl::single(u"Pawgram Desktop"_q),
					st::boxTitle),
				QMargins(0, 12, 0, 0),
				style::al_top);
			raw->add(
				object_ptr<Ui::FlatLabel>(
					raw,
					rpl::single(u"v7.0.9 (Официальная кастомная сборка)"_q),
					st::defaultFlatLabel),
				QMargins(0, 4, 0, 14),
				style::al_top);
			return result;
		},
	});
}

struct CategoryCard {
	QString title;
	QString about;
	const style::icon *icon = nullptr;
	Type (*target)() = nullptr;
};

void AddCategoryGrid(SectionBuilder &builder) {
	const auto controller = builder.controller();
	if (!controller) {
		return;
	}
	const auto showOther = builder.showOther();
	builder.add([=](const WidgetContext &ctx) {
		const auto cards = std::make_shared<std::vector<CategoryCard>>(
			std::vector<CategoryCard>{
			{
				u"Приватность"_q,
				u"Невидимка, чистка ссылок, скрытие историй"_q,
				&st::menuIconLock,
				CustomPrivacySection::Id,
			},
			{
				u"Сообщения"_q,
				u"Стикеры, медиа, кэш, ускорение загрузок"_q,
				&st::menuIconChatBubble,
				CustomMessagesSection::Id,
			},
			{
				u"Сеть и прокси"_q,
				u"tg-ws-proxy, Cloudflare, обход блокировок"_q,
				&st::menuIconNetwork,
				CustomProxySection::Id,
			},
			{
				u"Интерфейс"_q,
				u"Игровой оверлей, без рекламы, стиль"_q,
				&st::menuIconPalette,
				CustomInterfaceSection::Id,
			},
			{
				u"Безопасность"_q,
				u"Блокировка Win+L, экстренная очистка tdata"_q,
				&st::menuIconDelete,
				CustomPanicSection::Id,
			},
			{
				u"О клиенте"_q,
				u"Pawgram Desktop, оптимизация RAM, инфо"_q,
				&st::menuIconCustomize,
				CustomAboutSection::Id,
			},
		});
		const auto count = int(cards->size());
		const auto rows = (count + 1) / 2;
		constexpr auto cardHeight = 118;
		constexpr auto skip = 10;
		auto result = object_ptr<Ui::FixedHeightWidget>(
			ctx.container,
			rows * cardHeight + (rows - 1) * skip);
		const auto grid = result.data();
		struct State {
			int over = -1;
			int pressed = -1;
		};
		const auto state = grid->lifetime().make_state<State>();

		const auto cardRect = [=](int index) {
			const auto width = (grid->width() - skip) / 2;
			return QRect(
				(index % 2) * (width + skip),
				(index / 2) * (cardHeight + skip),
				width,
				cardHeight);
		};
		const auto cardAt = [=](QPoint point) {
			for (auto i = 0; i != count; ++i) {
				if (cardRect(i).contains(point)) {
					return i;
				}
			}
			return -1;
		};
		grid->paintRequest() | rpl::on_next([=] {
			auto p = QPainter(grid);
			auto hq = PainterHighQualityEnabler(p);
			constexpr auto radius = 12;
			constexpr auto tile = 40;
			constexpr auto tileRadius = 10;
			constexpr auto padding = 12;
			constexpr auto titleSkip = 10;
			constexpr auto aboutSkip = 2;
			for (auto i = 0; i != count; ++i) {
				const auto rect = cardRect(i);
				p.setPen(Qt::NoPen);
				p.setBrush((i == state->over)
					? st::windowBgRipple
					: st::windowBgOver);
				p.drawRoundedRect(rect, radius, radius);
				const auto tileRect = QRect(
					rect.x() + padding,
					rect.y() + padding,
					tile,
					tile);
				p.setBrush(st::windowBgActive);
				p.drawRoundedRect(
					tileRect,
					tileRadius,
					tileRadius);
				(*cards)[i].icon->paintInCenter(
					p,
					tileRect,
					st::windowFgActive->c);
				const auto textLeft = rect.x() + padding;
				const auto textWidth = rect.width() - 2 * padding;
				auto top = tileRect.y() + tile + titleSkip;
				p.setPen(st::windowBoldFg);
				p.setFont(st::semiboldFont);
				p.drawText(
					QRect(textLeft, top, textWidth, st::semiboldFont->height),
					Qt::AlignLeft | Qt::AlignTop,
					st::semiboldFont->elided((*cards)[i].title, textWidth));
				top += st::semiboldFont->height + aboutSkip;
				p.setPen(st::windowSubTextFg);
				p.setFont(st::normalFont);
				p.drawText(
					QRect(
						textLeft,
						top,
						textWidth,
						rect.y() + rect.height() - top - padding / 2),
					Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
					(*cards)[i].about);
			}
		}, grid->lifetime());
		grid->setMouseTracking(true);
		grid->events(
		) | rpl::on_next([=](not_null<QEvent*> e) {
			const auto type = e->type();
			if (type == QEvent::MouseMove) {
				const auto over = cardAt(
					static_cast<QMouseEvent*>(e.get())->pos());
				if (state->over != over) {
					state->over = over;
					grid->setCursor((over >= 0)
						? style::cur_pointer
						: style::cur_default);
					grid->update();
				}
			} else if (type == QEvent::Leave) {
				state->over = -1;
				grid->update();
			} else if (type == QEvent::MouseButtonPress) {
				state->pressed = cardAt(
					static_cast<QMouseEvent*>(e.get())->pos());
			} else if (type == QEvent::MouseButtonRelease) {
				const auto index = cardAt(
					static_cast<QMouseEvent*>(e.get())->pos());
				const auto pressed = std::exchange(state->pressed, -1);
				if (index >= 0 && index == pressed) {
					const auto target = (*cards)[index].target();
					crl::on_main(controller, [=] {
						showOther(target);
					});
				}
			}
		}, grid->lifetime());
		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result),
			.margin = QMargins(16, 12, 16, 4),
		};
	});
}

void AddCategorySearch(
		SectionBuilder &builder,
		std::vector<Type> sections,
		FnMut<void()> content) {
	const auto query = std::make_shared<rpl::variable<QString>>();
	if (const auto controller = builder.controller()) {
		const auto showOther = builder.showOther();
		builder.add([=](const WidgetContext &ctx) {
			auto result = object_ptr<Ui::VerticalLayout>(ctx.container);
			const auto container = result.data();

			auto searchController = std::make_unique<Ui::SearchFieldController>(QString());
			auto rowView = searchController->createRowView(
				container,
				st::infoLayerMediaSearch);
			const auto row = rowView.wrap.release();
			row->show();
			container->add(
				object_ptr<Ui::RpWidget>::fromRaw(row),
				QMargins(16, 12, 16, 4));

			const auto results = container->add(
				object_ptr<Ui::VerticalLayout>(container));

			const auto searchState = container->lifetime().make_state<
				std::unique_ptr<Ui::SearchFieldController>>(
					std::move(searchController));

			(*searchState)->queryChanges(
			) | rpl::on_next([=](const QString &value) {
				*query = value;
				FillSearchResults(
					results,
					controller,
					showOther,
					sections,
					value);
			}, row->lifetime());

			return SectionBuilder::WidgetToAdd{ .widget = std::move(result) };
		});
	}
	builder.scope(std::move(content), query->value(
	) | rpl::map([](const QString &value) {
		return value.trimmed().isEmpty();
	}));
}

// Главный раздел Pawgram
const auto kCustomMeta = BuildHelper({
	.id = CustomSection::Id(),
	.parentId = MainId(),
	.title = &tr::lng_settings_features,
	.icon = &st::menuIconCustomize,
}, [](SectionBuilder &builder) {
	AddCategorySearch(builder, AllCustomSections(), [&] {
		AddHeader(builder);
		AddCategoryGrid(builder);
	});
});

CustomSection::CustomSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomSection::title() {
	return rpl::single(u"Pawgram"_q);
}

void CustomSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kCustomMeta.build);
	Ui::ResizeFitChild(this, content);
}

} // namespace

Type CustomSettingsId() {
	return CustomSection::Id();
}

} // namespace Settings
