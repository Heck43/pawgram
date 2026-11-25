/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "lang/lang_keys.h"
#include "settings/sections/settings_custom.h"

#include "data/data_session.h"
#include "data/data_document.h"
#include "data/stickers/data_stickers.h"
#include "data/stickers/data_stickers_set.h"
#include "apiwrap.h"
#include "storage/storage_account.h"

#include "custom_features/custom_settings.hpp"
#include "custom_features/downloads_router.hpp"
#include "custom_features/session_wipe.hpp"
#include "custom_features/in_game_overlay.hpp"
#include "custom_features/ws_proxy_manager.hpp"
#include "custom_features/memory_optimizer.hpp"
#include "base/timer.h"
#include "boxes/connection_box.h"
#include "settings/settings_builder.h"
#include "settings/settings_common.h"
#include "settings/settings_common_session.h"
#include "settings/sections/settings_main.h"
#include "core/application.h"
#include "core/core_settings.h"
#include "core/file_utilities.h"
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
#include <QtCore/QDateTime>
#include <QtGui/QFontMetrics>
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

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <vector>

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

class CustomPerformanceSection : public Section<CustomPerformanceSection> {
public:
	CustomPerformanceSection(
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

[[nodiscard]] QString FormatMb(qint64 kb) {
	return QString::number(static_cast<double>(kb) / 1024.0, 'f', 1) + u" МБ"_q;
}

void ShowStickerPrecacheDialog(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Предзагрузка стикеров"_q));

		const auto session = &controller->session();
		auto &stickers = session->data().stickers();
		const auto &active = stickers.setsOrder();
		const auto &archived = stickers.archivedSetsOrder();

		struct PrecacheState {
			std::shared_ptr<bool> cancelled = std::make_shared<bool>(false);
			std::vector<uint64> pendingIds;
			int totalToRequest = 0;
			int requestedBatches = 0;
			int totalSetsCount = 0;
			qint64 lastProgressMs = 0;
			bool finished = false;
			bool stalledWarned = false;
			Ui::FlatLabel *statusLabel = nullptr;
			Ui::FlatLabel *detailLabel = nullptr;
			base::Timer batchTimer;
			base::Timer watchTimer;
		};

		const auto state = box->lifetime().make_state<PrecacheState>();
		state->totalSetsCount = int(active.size() + archived.size());
		state->lastProgressMs = QDateTime::currentMSecsSinceEpoch();
		std::unordered_set<uint64> seenIds;

		const auto collectSet = [&](uint64 setId) {
			if (!seenIds.insert(setId).second) {
				return;
			}
			const auto &sets = session->data().stickers().sets();
			const auto it = sets.find(setId);
			if (it == sets.end()) {
				return;
			}
			const auto set = it->second.get();
			if (set->stickers.empty() && (set->flags & Data::StickersSetFlag::NotLoaded)) {
				state->pendingIds.push_back(setId);
			}
			if (set->hasThumbnail()) {
				set->loadThumbnail();
			}
			for (const auto cover : set->covers) {
				if (cover->sticker()) {
					cover->loadThumbnail(cover->stickerSetOrigin());
				}
			}
		};

		for (const auto id : active) {
			collectSet(id);
		}
		for (const auto id : archived) {
			collectSet(id);
		}
		state->totalToRequest = int(state->pendingIds.size());

		const auto layout = box->verticalLayout();
		state->statusLabel = layout->add(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(QString()),
				st::boxLabel),
			st::boxRowPadding);
		state->detailLabel = layout->add(
			object_ptr<Ui::FlatLabel>(
				box,
				rpl::single(QString()),
				st::defaultFlatLabel),
			st::boxRowPadding);

		const auto countRemaining = [=] {
			int remaining = 0;
			const auto &sets = session->data().stickers().sets();
			for (const auto setId : state->pendingIds) {
				const auto it = sets.find(setId);
				if (it != sets.end()) {
					const auto set = it->second.get();
					if (set->stickers.empty() && (set->flags & Data::StickersSetFlag::NotLoaded)) {
						++remaining;
					}
				}
			}
			return remaining;
		};

		const auto updateUi = [=] {
			if (*state->cancelled || state->finished) {
				return;
			}
			if (state->totalToRequest == 0) {
				state->finished = true;
				state->statusLabel->setText(
					u"Все наборы уже находятся в локальном кэше (%1)."_q.arg(state->totalSetsCount));
				state->detailLabel->setText(u"Локальная база стикеров актуальна."_q);
				session->local().writeArchivedStickers();
				session->local().writeInstalledStickers();
				return;
			}

			const int remaining = countRemaining();
			const int done = std::max(0, state->totalToRequest - remaining);
			state->statusLabel->setText(
				u"Загружено наборов: %1 из %2"_q.arg(done).arg(state->totalToRequest));

			if (remaining == 0) {
				state->finished = true;
				state->batchTimer.cancel();
				state->watchTimer.cancel();
				state->detailLabel->setText(u"Кэширование завершено, данные сохранены на диск."_q);
				session->local().writeArchivedStickers();
				session->local().writeInstalledStickers();
			} else if (state->stalledWarned) {
				state->detailLabel->setText(
					u"Сервер отвечает медленно или нет сети. Можно подождать или отменить."_q);
			} else {
				state->detailLabel->setText(
					u"Пакетная загрузка миниатюр и метаданных (без перегрузки сети)..."_q);
			}
		};

		session->data().stickers().updated(
		) | rpl::on_next([=](Data::StickersType) {
			if (*state->cancelled) {
				return;
			}
			state->lastProgressMs = QDateTime::currentMSecsSinceEpoch();
			state->stalledWarned = false;
			updateUi();
		}, box->lifetime());

		const auto nextBatchIndex = std::make_shared<int>(0);
		constexpr int kBatchSize = 10;

		const auto scheduleNextBatch = [=] {
			if (*state->cancelled || state->finished) {
				state->batchTimer.cancel();
				return;
			}
			const int start = *nextBatchIndex;
			if (start >= state->totalToRequest) {
				state->batchTimer.cancel();
				return;
			}
			const int end = std::min(start + kBatchSize, state->totalToRequest);
			const auto &sets = session->data().stickers().sets();
			int scheduledInBatch = 0;
			for (int i = start; i < end; ++i) {
				const auto it = sets.find(state->pendingIds[i]);
				if (it != sets.end()) {
					const auto set = it->second.get();
					if (set->stickers.empty() && (set->flags & Data::StickersSetFlag::NotLoaded)) {
						session->api().scheduleStickerSetRequest(set->id, set->accessHash);
						++scheduledInBatch;
					}
				}
			}
			*nextBatchIndex = end;
			if (scheduledInBatch > 0) {
				session->api().requestStickerSets();
				state->lastProgressMs = QDateTime::currentMSecsSinceEpoch();
			}
			if (*nextBatchIndex >= state->totalToRequest) {
				state->batchTimer.cancel();
			}
			updateUi();
		};

		state->batchTimer.setCallback(scheduleNextBatch);
		state->watchTimer.setCallback([=] {
			if (*state->cancelled || state->finished) {
				return;
			}
			const qint64 now = QDateTime::currentMSecsSinceEpoch();
			if (now - state->lastProgressMs >= 15000) {
				state->stalledWarned = true;
			}
			updateUi();
		});

		if (state->totalToRequest > 0) {
			scheduleNextBatch();
			state->batchTimer.callEach(350);
			state->watchTimer.callEach(1000);
		} else {
			updateUi();
		}

		box->boxClosing(
		) | rpl::on_next([=] {
			*state->cancelled = true;
			state->batchTimer.cancel();
			state->watchTimer.cancel();
		}, box->lifetime());

		box->addButton(tr::lng_cancel(), [=] {
			*state->cancelled = true;
			state->batchTimer.cancel();
			state->watchTimer.cancel();
			box->closeBox();
		});
	}));
}

void ShowDownloadCategoriesDialog(not_null<Window::SessionController*> controller) {
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Категории умных загрузок"_q));
		const auto layout = box->verticalLayout();

		const auto rawDownloadPath = Core::App().settings().downloadPath();
		const auto effectiveDefaultPath = (rawDownloadPath == FileDialog::Tmp())
			? controller->session().local().tempDirectory()
			: rawDownloadPath;
		const auto basePath = CustomFeatures::DownloadsRouter::ResolveBasePath(
			effectiveDefaultPath);
		Ui::AddDividerText(
			layout,
			rpl::single(u"Базовая папка: %1\nОтключённые категории сохраняются напрямую в папку загрузок Telegram."_q.arg(basePath)));

		auto addCategoryCheck = [&](const QString &label, bool &target) {
			const auto check = layout->add(
				object_ptr<Ui::Checkbox>(box, label, target, st::defaultCheckbox),
				st::boxRowPadding);
			check->checkedChanges(
			) | rpl::on_next([&target](bool checked) {
				target = checked;
				CustomFeatures::GetConfig().save();
			}, check->lifetime());
		};

		auto &cfg = CustomFeatures::GetConfig();
		addCategoryCheck(u"Изображения (Images)"_q, cfg.downloadsRouteImages);
		addCategoryCheck(u"Видео (Video)"_q, cfg.downloadsRouteVideo);
		addCategoryCheck(u"Музыка и аудио (Audio)"_q, cfg.downloadsRouteAudio);
		addCategoryCheck(u"Документы и книги (Documents)"_q, cfg.downloadsRouteDocuments);
		addCategoryCheck(u"Архивы (Archives)"_q, cfg.downloadsRouteArchives);
		addCategoryCheck(u"Программы (Programs)"_q, cfg.downloadsRoutePrograms);
		addCategoryCheck(u"Разное (Other)"_q, cfg.downloadsRouteOther);

		if (!cfg.downloadsCustomBasePath.trimmed().isEmpty()) {
			box->addButton(rpl::single(u"Сбросить кастомный путь"_q), [=] {
				CustomFeatures::GetConfig().downloadsCustomBasePath.clear();
				CustomFeatures::GetConfig().save();
				box->closeBox();
			});
		}

		box->addButton(tr::lng_close(), [=] { box->closeBox(); });
	}));
}

// 1. Приватность и Безопасность
const auto kPrivacyMeta = BuildHelper({
	.id = CustomPrivacySection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_privacy,
	.icon = &st::menuIconLock,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Ссылки и лента"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/clean_urls"_q,
		.title = rpl::single(u"Очищать UTM-метки и трекеры в ссылках"_q),
		.checked = CustomFeatures::GetConfig().cleanTrackingUrls,
		.keywords = { u"clean"_q, u"url"_q, u"utm"_q, u"tracking"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().cleanTrackingUrls = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	if (const auto check = builder.addCheckbox({
		.id = u"custom/direct_links"_q,
		.title = rpl::single(u"Открывать внешние ссылки без подтверждения"_q),
		.checked = CustomFeatures::GetConfig().directExternalLinks,
		.keywords = { u"links"_q, u"external"_q, u"direct"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().directExternalLinks = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	if (const auto check = builder.addCheckbox({
		.id = u"custom/hide_stories"_q,
		.title = rpl::single(u"Скрыть панель историй (Stories)"_q),
		.checked = CustomFeatures::GetConfig().hideStoriesBar,
		.keywords = { u"stories"_q, u"hide"_q, u"privacy"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().hideStoriesBar = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(
		u"Вырезает utm_*, fbclid, gclid, yclid при копировании, убирает лишний диалог перехода по ссылкам и скрывает верхнюю ленту историй."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Блокировка"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/auto_lock"_q,
		.title = rpl::single(u"Блокировать клиент при Win + L"_q),
		.checked = CustomFeatures::GetConfig().autoLockOnWindowsLock,
		.keywords = { u"lock"_q, u"security"_q, u"win+l"_q, u"passcode"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().autoLockOnWindowsLock = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addButton({
		.id = u"custom/btn_passcode"_q,
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
	builder.addDividerText(rpl::single(
		u"Автоматическая блокировка срабатывает при блокировке экрана Windows, если в клиенте задан код-пароль."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Экстренная очистка (Panic Wipe)"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/auto_wipe"_q,
		.title = rpl::single(u"Полная очистка профиля при выходе из аккаунта"_q),
		.checked = CustomFeatures::GetConfig().autoWipeOnLogout,
		.keywords = { u"wipe"_q, u"logout"_q, u"tdata"_q, u"privacy"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().autoWipeOnLogout = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(
		u"Автоматически стирает tdata и кэш текущего профиля (включая все авторизованные аккаунты в нём) при выходе из любого аккаунта."_q));

	builder.addButton({
		.id = u"custom/btn_panic_wipe"_q,
		.title = rpl::single(u"Очистить сессию и выйти (Panic Wipe)"_q),
		.icon = &st::menuIconLeaveAttention,
		.onClick = [=] {
			builder.controller()->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(rpl::single(u"Экстренная очистка данных"_q));

				const auto layout = box->verticalLayout();

				Ui::AddDividerText(
					layout,
					rpl::single(u"Внимание! Это действие отправит запрос на завершение сеансов на серверы Telegram (без гарантии доставки при отсутствии сети) и безвозвратно удалит локальные данные текущего профиля:"_q));

				Ui::AddDividerText(
					layout,
					rpl::single(u"• Папку аккаунта и ключи сессий (tdata)\n• Историю сообщений, кэш и базы данных\n• Все файлы логов (log.txt, DebugLogs)\n• Данные tg-ws-proxy\n• Локальные настройки клиента"_q));

				const auto accountsCount = CustomFeatures::SessionWipeDetail::CountAuthorizedAccounts();
				if (accountsCount > 1) {
					Ui::AddDividerText(
						layout,
						rpl::single(u"Внимание: в этом профиле авторизовано несколько аккаунтов (%1). Очистка отправит запрос выхода и удалит локальные данные сразу для всех них."_q.arg(accountsCount)));
				}

				const auto canonicalWork = CustomFeatures::SessionWipeDetail::ResolveCanonicalLocalDirectory(cWorkingDir());
				const auto displayWork = canonicalWork.isEmpty()
					? QDir::toNativeSeparators(cWorkingDir())
					: canonicalWork;
				const auto scopeText = CustomFeatures::SessionWipeDetail::IsStandardAppDataWorkDir(canonicalWork)
					? u"Режим профиля: стандартный (%1)."_q.arg(displayWork)
					: u"Режим профиля: Portable / -workdir (%1). Стандартная папка AppData затронута не будет."_q.arg(displayWork);
				Ui::AddDividerText(layout, rpl::single(scopeText));

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

CustomPrivacySection::CustomPrivacySection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomPrivacySection::title() {
	return rpl::single(u"Приватность"_q);
}

void CustomPrivacySection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kPrivacyMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 2. Сообщения, Стикеры и Загрузки
const auto kMessagesMeta = BuildHelper({
	.id = CustomMessagesSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_chat_settings,
	.icon = &st::menuIconChatBubble,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Стикеры и эмодзи"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/unlimited_stickers"_q,
		.title = rpl::single(u"Расширенный список недавних стикеров"_q),
		.checked = CustomFeatures::GetConfig().unlimitedRecentStickers,
		.keywords = { u"stickers"_q, u"recent"_q, u"unlimited"_q, u"лимит"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().unlimitedRecentStickers = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addSubsectionTitle({
		.id = u"custom/slider_recent_stickers"_q,
		.title = rpl::single(u"Лимит недавних стикеров (20–300)"_q),
		.keywords = { u"stickers"_q, u"recent"_q, u"limit"_q, u"лимит"_q },
	});
	builder.add([](const WidgetContext &ctx) {
		auto result = MakeSliderWithLabel(
			ctx.container.get(),
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("300 шт."),
			false);

		const auto slider = result.slider;
		const auto label = result.label;

		const int current = std::clamp(
			CustomFeatures::GetConfig().recentStickersMaxCount,
			20,
			300);
		const int currentSnapped = ((current - 20 + 10) / 20) * 20 + 20;
		slider->setPseudoDiscrete(
			15,
			[](int idx) { return (idx + 1) * 20; },
			std::clamp(currentSnapped, 20, 300),
			[=](int val) {
				CustomFeatures::GetConfig().recentStickersMaxCount = std::clamp(val, 20, 300);
				CustomFeatures::GetConfig().save();
				label->setText(QString::number(val) + u" шт."_q);
			});

		label->setText(QString::number(std::clamp(currentSnapped, 20, 300)) + u" шт."_q);

		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result.widget),
			.margin = st::settingsScalePadding,
		};
	});
	builder.addSkip(st::settingsCheckboxesSkip);

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

	builder.addButton({
		.id = u"custom/btn_precache_stickers"_q,
		.title = rpl::single(u"Предзагрузить все стикеры в кэш"_q),
		.icon = &st::menuIconDownload,
		.onClick = [=] {
			if (const auto controller = builder.controller()) {
				ShowStickerPrecacheDialog(controller);
			}
		},
		.keywords = { u"stickers"_q, u"cache"_q, u"preload"_q, u"offline"_q },
	});
	builder.addDividerText(rpl::single(
		u"Позволяет задать лимит недавних стикеров (20–300), отображать архивированные наборы без ограничения в 200 паков и пакетно кэшировать стикеры с возможностью отмены."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Умные загрузки"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/downloads_router"_q,
		.title = rpl::single(u"Сортировать загрузки по папкам"_q),
		.checked = CustomFeatures::GetConfig().enableDownloadsRouter,
		.keywords = { u"downloads"_q, u"router"_q, u"folders"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableDownloadsRouter = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addButton({
		.id = u"custom/btn_download_categories"_q,
		.title = rpl::single(u"Настроить категории загрузок"_q),
		.icon = &st::menuIconShowInFolder,
		.onClick = [=] {
			if (const auto controller = builder.controller()) {
				ShowDownloadCategoriesDialog(controller);
			}
		},
		.keywords = { u"downloads"_q, u"categories"_q, u"folders"_q, u"категории"_q },
	});
	builder.addDividerText(rpl::single(
		u"Раскладывает сохраняемые файлы по подпапкам внутри выбранной папки загрузок Telegram с безопасным откатом при ошибках доступа."_q));
});

CustomMessagesSection::CustomMessagesSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomMessagesSection::title() {
	return rpl::single(u"Сообщения"_q);
}

void CustomMessagesSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kMessagesMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 3. Сеть и Прокси
const auto kProxyMeta = BuildHelper({
	.id = CustomProxySection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_advanced,
	.icon = &st::menuIconNetwork,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Встроенный WebSocket-прокси"_q));

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
		u"Локальный туннель на базе tg-ws-proxy (Flowseal) поверх WebSocket/Cloudflare CDN. Запускается автоматически в фоне и привязан к текущему экземпляру Pawgram."_q));

	builder.addButton({
		.id = u"custom/btn_ws_proxy_status"_q,
		.title = rpl::single(u"Статус и управление tg-ws-proxy"_q),
		.icon = &st::menuIconNetwork,
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			if (!CustomFeatures::WsProxyManager::Instance().hasExecutable()) {
				controller->show(Ui::MakeConfirmBox({
					.text = u"Файл tg-ws-proxy.exe не найден рядом с клиентом.\n\nСкачать его из репозитория Flowseal/tg-ws-proxy?"_q,
					.confirmed = [=](Fn<void()> close) {
						close();
						controller->showToast(u"Скачивание tg-ws-proxy..."_q);
						CustomFeatures::WsProxyManager::Instance().downloadProxyAsync([=](bool ok, QString err) {
							if (ok) {
								if (CustomFeatures::GetConfig().enableWsProxy) {
									CustomFeatures::WsProxyManager::Instance().startProxy();
								}
								controller->showToast(u"tg-ws-proxy успешно загружен."_q);
							} else {
								const auto msg = err.trimmed().isEmpty()
									? u"Не удалось скачать tg-ws-proxy."_q
									: u"Ошибка загрузки tg-ws-proxy: %1"_q.arg(err);
								controller->showToast(msg);
							}
						});
					},
					.confirmText = u"Скачать"_q,
					.cancelText = tr::lng_cancel(),
				}));
			} else {
				controller->show(Box([=](not_null<Ui::GenericBox*> box) {
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

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Системные настройки"_q));

	builder.addButton({
		.id = u"custom/btn_tg_proxies"_q,
		.title = rpl::single(u"Список прокси Telegram"_q),
		.icon = &st::menuIconNetwork,
		.onClick = [=] {
			if (const auto controller = builder.controller()) {
				ProxiesBoxController::Show(controller);
			}
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
	return rpl::single(u"Сеть"_q);
}

void CustomProxySection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kProxyMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 4. Интерфейс, Стиль и Оверлей
const auto kInterfaceMeta = BuildHelper({
	.id = CustomInterfaceSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_features,
	.icon = &st::menuIconPalette,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Чистый интерфейс и тема"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/hide_ads"_q,
		.title = rpl::single(u"Скрывать спонсорские сообщения в каналах и ботах"_q),
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
		.id = u"custom/windows_accent"_q,
		.title = rpl::single(u"Синхронизировать цвет акцента с Windows"_q),
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

	if (const auto check = builder.addCheckbox({
		.id = u"custom/soft_shadows"_q,
		.title = rpl::single(u"Мягкие тени под элементами интерфейса"_q),
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
	builder.addSubsectionTitle(rpl::single(u"Скругление углов"_q));

	builder.addSubsectionTitle({
		.id = u"custom/slider_msg_radius"_q,
		.title = rpl::single(u"Сообщения (0–24 px)"_q),
		.keywords = { u"radius"_q, u"message"_q, u"скругление"_q },
	});
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

		const int currentRadius = std::clamp(
			CustomFeatures::GetConfig().messageBorderRadius,
			0,
			24);
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

	builder.addSubsectionTitle({
		.id = u"custom/slider_chat_radius"_q,
		.title = rpl::single(u"Чаты в списке (0–24 px)"_q),
		.keywords = { u"radius"_q, u"chat"_q, u"скругление"_q },
	});
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

		const int currentRadius = std::clamp(
			CustomFeatures::GetConfig().chatBorderRadius,
			0,
			24);
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
	builder.addDividerText(rpl::single(
		u"Изменения радиуса скругления применяются после перезапуска Pawgram."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Прокрутка и анимации"_q));

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
		.title = rpl::single(u"Адаптивная инерция прокрутки (Fluid Scroll)"_q),
		.checked = CustomFeatures::GetConfig().enableMotionBlur,
		.keywords = { u"motion"_q, u"blur"_q, u"smooth"_q, u"animation"_q, u"hz"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().enableMotionBlur = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addSubsectionTitle({
		.id = u"custom/slider_scroll_inertia"_q,
		.title = rpl::single(u"Интенсивность инерции прокрутки"_q),
		.keywords = { u"scroll"_q, u"inertia"_q, u"прокрутка"_q },
	});
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

		const int currentIntensity = std::clamp(
			CustomFeatures::GetConfig().motionBlurIntensity,
			1,
			10);
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

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Игровой оверлей"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/game_overlay"_q,
		.title = rpl::single(u"Игровой оверлей (Shift + ~ / Shift + F11)"_q),
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
		.id = u"custom/btn_overlay_games"_q,
		.title = rpl::single(u"Выбрать игры для оверлея"_q),
		.icon = &st::menuIconDevices,
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			controller->show(Box([=](not_null<Ui::GenericBox*> box) {
				box->setTitle(rpl::single(u"Игры для оверлея"_q));

				const auto layout = box->verticalLayout();

				Ui::AddDividerText(layout, rpl::single(
					u"Оверлей открывается по горячим клавишам Shift + ~ или Shift + F11:"_q));

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
				Ui::AddSubsectionTitle(layout, rpl::single(u"Сохранённые процессы (нажмите для удаления)"_q));

				if (CustomFeatures::GetConfig().overlayAllowedGames.isEmpty()) {
					const auto emptyHint = CustomFeatures::GetConfig().overlayAllGames
						? u"Список пуст — оверлей разрешён поверх всех игр и приложений."_q
						: u"Список пуст — оверлей сейчас не разрешён ни для одной игры. Добавьте процесс ниже или включите режим «поверх всех игр»."_q;
					Ui::AddDividerText(layout, rpl::single(emptyHint));
				} else {
					for (const auto &game : CustomFeatures::GetConfig().overlayAllowedGames) {
						const auto btn = layout->add(object_ptr<Ui::SettingsButton>(
							box,
							rpl::single(game + u"   [Удалить]"_q),
							st::settingsButtonNoIcon));
						btn->setClickedCallback([=] {
							CustomFeatures::GetConfig().overlayAllowedGames.removeAll(game);
							CustomFeatures::GetConfig().save();
							box->closeBox();
						});
					}
				}

				Ui::AddSkip(layout);
				Ui::AddSubsectionTitle(layout, rpl::single(u"Добавить из запущенных окон"_q));

				const auto running = CustomFeatures::GetRunningUserApps();
				if (running.isEmpty()) {
					Ui::AddDividerText(layout, rpl::single(u"Запущенных пользовательских окон не найдено."_q));
				} else {
					for (const auto &app : running) {
						const bool already = CustomFeatures::GetConfig().overlayAllowedGames.contains(
							app.exeName,
							Qt::CaseInsensitive);
						const auto itemTitle = (already ? u"✓ "_q : u"+ "_q)
							+ app.windowTitle
							+ u" ("_q
							+ app.exeName
							+ u")"_q;
						const auto btn = layout->add(object_ptr<Ui::SettingsButton>(
							box,
							rpl::single(itemTitle),
							st::settingsButtonNoIcon));
						btn->setClickedCallback([=] {
							if (!CustomFeatures::GetConfig().overlayAllowedGames.contains(
									app.exeName,
									Qt::CaseInsensitive)) {
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
});

CustomInterfaceSection::CustomInterfaceSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomInterfaceSection::title() {
	return rpl::single(u"Интерфейс"_q);
}

void CustomInterfaceSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kInterfaceMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 5. Производительность (Память и запуск)
const auto kPerformanceMeta = BuildHelper({
	.id = CustomPerformanceSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_advanced,
	.icon = &st::menuIconBoosts,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"Запуск клиента"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/fast_startup"_q,
		.title = rpl::single(u"Быстрый запуск (отложенная загрузка стикеров)"_q),
		.checked = CustomFeatures::GetConfig().fastStartup
			&& CustomFeatures::GetConfig().delayStickersLoad,
		.keywords = { u"startup"_q, u"fast"_q, u"stickers"_q, u"launch"_q, u"запуск"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().fastStartup = checked;
			CustomFeatures::GetConfig().delayStickersLoad = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}
	builder.addDividerText(rpl::single(
		u"Откладывает чтение локальных баз стикеров и эмодзи на 500 мс после открытия главного окна. Изменение применяется при следующем запуске."_q));

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Графический кэш Qt (QPixmapCache)"_q));

	builder.addSubsectionTitle({
		.id = u"custom/slider_pixmap_cache"_q,
		.title = rpl::single(u"Лимит кэша изображений (8–256 МБ)"_q),
		.keywords = { u"pixmap"_q, u"cache"_q, u"кэш"_q, u"память"_q },
	});
	builder.add([](const WidgetContext &ctx) {
		auto result = MakeSliderWithLabel(
			ctx.container.get(),
			st::settingsScale,
			st::settingsScaleLabel,
			st::normalFont->spacew * 2,
			st::settingsScaleLabel.style.font->width("256 МБ"),
			false);

		const auto slider = result.slider;
		const auto label = result.label;

		const int currentMb = std::clamp(
			CustomFeatures::GetConfig().pixmapCacheLimitMb,
			8,
			256);
		const int currentSnapped = ((currentMb - 8 + 4) / 8) * 8 + 8;
		slider->setPseudoDiscrete(
			32,
			[](int idx) { return (idx + 1) * 8; },
			std::clamp(currentSnapped, 8, 256),
			[=](int val) {
				CustomFeatures::GetConfig().pixmapCacheLimitMb = std::clamp(val, 8, 256);
				CustomFeatures::GetConfig().save();
				CustomFeatures::MemoryOptimizer::ApplyCacheLimit();
				label->setText(QString::number(val) + u" МБ"_q);
			});

		label->setText(QString::number(std::clamp(currentSnapped, 8, 256)) + u" МБ"_q);

		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result.widget),
			.margin = st::settingsScalePadding,
		};
	});
	builder.addSkip(st::settingsCheckboxesSkip);

	builder.addButton({
		.id = u"custom/btn_clear_qt_cache"_q,
		.title = rpl::single(u"Очистить графический кэш Qt"_q),
		.icon = &st::menuIconClear,
		.onClick = [=] {
			CustomFeatures::MemoryOptimizer::ClearGraphicsCacheOnly();
			if (const auto controller = builder.controller()) {
				controller->showToast(u"Графический кэш Qt очищен."_q);
			}
		},
		.keywords = { u"pixmap"_q, u"cache"_q, u"qt"_q, u"кэш"_q },
	});

	builder.addDivider();
	builder.addSubsectionTitle(rpl::single(u"Оптимизация памяти (Windows)"_q));

	if (const auto check = builder.addCheckbox({
		.id = u"custom/trim_memory_on_minimize"_q,
		.title = rpl::single(u"Сбрасывать кэш и Working Set при сворачивании"_q),
		.checked = CustomFeatures::GetConfig().trimMemoryOnMinimize,
		.keywords = { u"ram"_q, u"memory"_q, u"minimize"_q, u"optimize"_q, u"tray"_q },
	})) {
		check->checkedChanges(
		) | rpl::on_next([=](bool checked) {
			CustomFeatures::GetConfig().trimMemoryOnMinimize = checked;
			CustomFeatures::GetConfig().save();
		}, check->lifetime());
	}

	builder.addButton({
		.id = u"custom/btn_trim_ram"_q,
		.title = rpl::single(u"Очистить кэш и сжать Working Set сейчас"_q),
		.icon = &st::menuIconBoosts,
		.onClick = [=] {
			const auto controller = builder.controller();
			const auto res = CustomFeatures::MemoryOptimizer::TrimWorkingSet(false);
			if (!controller) {
				return;
			}
			if (res.skippedByCooldown) {
				controller->showToast(
					u"Подождите %1 сек. перед повторным сжатием Working Set."_q.arg(res.cooldownRemainingSec));
			} else if (res.workingSetBeforeKb > 0) {
				controller->showToast(
					u"Working Set: %1 → %2 (Private: %3)"_q
						.arg(FormatMb(res.workingSetBeforeKb))
						.arg(FormatMb(res.workingSetAfterKb))
						.arg(FormatMb(res.privateBytesAfterKb)));
			} else {
				controller->showToast(u"Кэш Qt очищен и Working Set сжат."_q);
			}
		},
		.keywords = { u"ram"_q, u"memory"_q, u"trim"_q, u"working set"_q, u"память"_q },
	});

	builder.addDividerText(rpl::single(
		u"Очищает QPixmapCache и просит Windows выгрузить неактивные страницы Working Set (защита от частых вызовов: 15 сек при сворачивании, 3 сек вручную). Это не освобождает активно используемую приватную память."_q));
});

CustomPerformanceSection::CustomPerformanceSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomPerformanceSection::title() {
	return rpl::single(u"Производительность"_q);
}

void CustomPerformanceSection::setupContent() {
	const auto content = Ui::CreateChild<Ui::VerticalLayout>(this);
	build(content, kPerformanceMeta.build);
	Ui::ResizeFitChild(this, content);
}

// 6. Дополнительно
const auto kAboutMeta = BuildHelper({
	.id = CustomAboutSection::Id(),
	.parentId = CustomSection::Id(),
	.title = &tr::lng_settings_section_info,
	.icon = &st::menuIconManage,
}, [](SectionBuilder &builder) {
	builder.addSubsectionTitle(rpl::single(u"О проекте Pawgram"_q));
	builder.addDividerText(rpl::single(
		u"Pawgram Desktop v7.0.9 — кастомная сборка на базе Telegram Desktop 7.0.9.\n\n"
		"Доступные модули:\n"
		"• Очистка UTM/трекеров, прямые ссылки, блокировка Win+L и Panic Wipe\n"
		"• Настраиваемый лимит недавних стикеров и пакетное кэширование\n"
		"• Умная сортировка загрузок по категориям\n"
		"• Встроенный локальный прокси tg-ws-proxy\n"
		"• Игровой оверлей, скругления и оптимизация кэша Qt / Working Set"_q));

	builder.addSubsectionTitle(rpl::single(u"Конфигурация"_q));
	builder.addDividerText(rpl::single(
		u"Файл настроек Pawgram:\n%1"_q.arg(
			QDir::toNativeSeparators(CustomFeatures::ClientConfig::getSettingsFilePath()))));

	builder.addButton({
		.id = u"custom/btn_reset_defaults"_q,
		.title = rpl::single(u"Сбросить настройки Pawgram по умолчанию"_q),
		.icon = &st::menuIconDelete,
		.onClick = [=] {
			const auto controller = builder.controller();
			if (!controller) {
				return;
			}
			controller->show(Ui::MakeConfirmBox({
				.text = u"Вы действительно хотите сбросить все кастомные настройки Pawgram к значениям по умолчанию?"_q,
				.confirmed = [=](Fn<void()> close) {
					close();
					CustomFeatures::GetConfig() = CustomFeatures::ClientConfig();
					CustomFeatures::GetConfig().save();
					CustomFeatures::MemoryOptimizer::ApplyCacheLimit();
					CustomFeatures::InGameOverlayManager::Instance().updateState();
					controller->showToast(u"Настройки Pawgram сброшены по умолчанию."_q);
				},
				.confirmText = u"Сбросить"_q,
			}));
		},
		.keywords = { u"reset"_q, u"default"_q, u"сброс"_q },
	});
});

CustomAboutSection::CustomAboutSection(
	QWidget *parent,
	not_null<Window::SessionController*> controller)
: Section(parent, controller) {
	setupContent();
}

rpl::producer<QString> CustomAboutSection::title() {
	return rpl::single(u"Дополнительно"_q);
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
		CustomPerformanceSection::Id(),
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
			raw->add(
				object_ptr<Ui::FlatLabel>(
					raw,
					rpl::single(u"Pawgram Settings"_q),
					st::boxTitle),
				QMargins(16, 12, 16, 0));
			raw->add(
				object_ptr<Ui::FlatLabel>(
					raw,
					rpl::single(u"Настрой свой Telegram под себя"_q),
					st::defaultFlatLabel),
				QMargins(16, 4, 16, 4));
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

class CategoryGridWidget final : public Ui::RpWidget {
public:
	CategoryGridWidget(
		QWidget *parent,
		not_null<Window::SessionController*> controller,
		Fn<void(Type)> showOther,
		std::vector<CategoryCard> cards)
	: Ui::RpWidget(parent)
	, _controller(controller)
	, _showOther(std::move(showOther))
	, _cards(std::move(cards)) {
		setMouseTracking(true);
	}

protected:
	int resizeGetHeight(int newWidth) override {
		updateLayout(newWidth);
		const int rows = (int(_cards.size()) + _columns - 1) / _columns;
		return (rows > 0)
			? (rows * _cardHeight + (rows - 1) * kSkip)
			: 0;
	}

	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		auto hq = PainterHighQualityEnabler(p);

		constexpr int kRadius = 12;
		constexpr int kPadding = 16;
		constexpr int kIconSize = 24;
		constexpr int kTitleGap = 10;
		constexpr int kAboutGap = 4;

		for (int i = 0; i < int(_cards.size()); ++i) {
			const auto rect = cardRect(i);
			const bool hovered = (i == _over);
			p.setBrush(hovered ? st::windowBgRipple : st::windowBgOver);
			p.setPen(QPen(
				hovered ? st::windowActiveTextFg->c : st::windowBgRipple->c,
				1));
			p.drawRoundedRect(rect.adjusted(0, 0, -1, -1), kRadius, kRadius);

			const auto iconRect = QRect(
				rect.x() + kPadding,
				rect.y() + kPadding,
				kIconSize,
				kIconSize);
			_cards[i].icon->paintInCenter(
				p,
				iconRect,
				st::windowActiveTextFg->c);

			const int textLeft = rect.x() + kPadding;
			const int textWidth = std::max(1, rect.width() - 2 * kPadding);
			int top = iconRect.y() + kIconSize + kTitleGap;

			p.setPen(st::windowBoldFg);
			p.setFont(st::semiboldFont);
			p.drawText(
				QRect(textLeft, top, textWidth, st::semiboldFont->height),
				Qt::AlignLeft | Qt::AlignTop,
				st::semiboldFont->elided(_cards[i].title, textWidth));

			top += st::semiboldFont->height + kAboutGap;
			p.setPen(st::windowSubTextFg);
			p.setFont(st::normalFont);
			p.drawText(
				QRect(
					textLeft,
					top,
					textWidth,
					std::max(1, rect.y() + rect.height() - top - kPadding)),
				Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				_cards[i].about);
		}
	}

	void mouseMoveEvent(QMouseEvent *e) override {
		const int over = cardAt(e->pos());
		if (_over != over) {
			_over = over;
			setCursor((over >= 0) ? style::cur_pointer : style::cur_default);
			update();
		}
	}

	void leaveEventHook(QEvent *e) override {
		if (_over != -1) {
			_over = -1;
			update();
		}
	}

	void mousePressEvent(QMouseEvent *e) override {
		if (e->button() == Qt::LeftButton) {
			_pressed = cardAt(e->pos());
		}
	}

	void mouseReleaseEvent(QMouseEvent *e) override {
		const int index = cardAt(e->pos());
		const int pressed = std::exchange(_pressed, -1);
		if (e->button() == Qt::LeftButton && index >= 0 && index == pressed) {
			const auto target = _cards[index].target();
			const auto controller = _controller;
			const auto showOther = _showOther;
			crl::on_main(controller, [=] {
				showOther(target);
			});
		}
	}

private:
	static constexpr int kSkip = 12;

	void updateLayout(int totalWidth) {
		_columns = (totalWidth >= 420) ? 2 : 1;
		const int cardWidth = std::max(
			120,
			(totalWidth - (_columns - 1) * kSkip) / _columns);
		const int textWidth = std::max(40, cardWidth - 32);
		const QFontMetrics titleFm(st::semiboldFont->f);
		const QFontMetrics aboutFm(st::normalFont->f);

		int maxHeight = 96;
		for (const auto &card : _cards) {
			const int aboutHeight = aboutFm.boundingRect(
				QRect(0, 0, textWidth, 1000),
				Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
				card.about).height();
			const int needed = 16 + 24 + 10 + titleFm.height() + 4 + aboutHeight + 16;
			if (needed > maxHeight) {
				maxHeight = needed;
			}
		}
		_cardHeight = maxHeight;
	}

	[[nodiscard]] QRect cardRect(int index) const {
		const int colWidth = (width() - (_columns - 1) * kSkip) / _columns;
		return QRect(
			(index % _columns) * (colWidth + kSkip),
			(index / _columns) * (_cardHeight + kSkip),
			colWidth,
			_cardHeight);
	}

	[[nodiscard]] int cardAt(QPoint point) const {
		for (int i = 0; i < int(_cards.size()); ++i) {
			if (cardRect(i).contains(point)) {
				return i;
			}
		}
		return -1;
	}

	not_null<Window::SessionController*> _controller;
	Fn<void(Type)> _showOther;
	std::vector<CategoryCard> _cards;
	int _columns = 2;
	int _cardHeight = 112;
	int _over = -1;
	int _pressed = -1;

};

void AddCategoryGrid(SectionBuilder &builder) {
	const auto controller = builder.controller();
	if (!controller) {
		return;
	}
	const auto showOther = builder.showOther();
	builder.add([=](const WidgetContext &ctx) {
		std::vector<CategoryCard> cards = {
			{
				u"Приватность"_q,
				u"Ссылки, блокировка, экстренная очистка"_q,
				&st::menuIconLock,
				CustomPrivacySection::Id,
			},
			{
				u"Сообщения"_q,
				u"Стикеры, лимиты, умные загрузки"_q,
				&st::menuIconChatBubble,
				CustomMessagesSection::Id,
			},
			{
				u"Сеть"_q,
				u"Встроенный tg-ws-proxy и подключения"_q,
				&st::menuIconNetwork,
				CustomProxySection::Id,
			},
			{
				u"Интерфейс"_q,
				u"Внешний вид, прокрутка и оверлей"_q,
				&st::menuIconPalette,
				CustomInterfaceSection::Id,
			},
			{
				u"Производительность"_q,
				u"Быстрый запуск, кэш Qt и память"_q,
				&st::menuIconBoosts,
				CustomPerformanceSection::Id,
			},
			{
				u"Дополнительно"_q,
				u"О клиенте, конфиг и сброс настроек"_q,
				&st::menuIconManage,
				CustomAboutSection::Id,
			},
		};

		auto result = object_ptr<CategoryGridWidget>(
			ctx.container,
			controller,
			showOther,
			std::move(cards));

		return SectionBuilder::WidgetToAdd{
			.widget = std::move(result),
			.margin = QMargins(16, 10, 16, 16),
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
