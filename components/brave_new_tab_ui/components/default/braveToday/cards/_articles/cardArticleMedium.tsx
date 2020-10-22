// Copyright (c) 2020 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// you can obtain one at http://mozilla.org/MPL/2.0/.

import * as React from 'react'
import CardImage from '../CardImage'
import * as Card from '../../cardSizes'
import PublisherMeta from '../PublisherMeta'
import useScrollIntoView from '../../useScrollIntoView'

// TODO(petemill): Large and Medium article should be combined to 1 component.

interface Props {
  content: (BraveToday.Article)[]
  publishers: BraveToday.Publishers
  articleToScrollTo?: BraveToday.FeedItem
  onReadFeedItem: (item: BraveToday.FeedItem) => any
}

type ArticleProps = {
  item: BraveToday.Article
  publisher: BraveToday.Publisher
  shouldScrollIntoView?: boolean
  onReadFeedItem: (item: BraveToday.FeedItem) => any
}

function MediumArticle (props: ArticleProps) {
  const { publisher, item } = props
  const [cardRef] = useScrollIntoView(props.shouldScrollIntoView || false)
  const onClick = React.useCallback(() => {
    props.onReadFeedItem(props.item)
  }, [props.onReadFeedItem, props.item])
  return (
    <Card.Small>
      <a onClick={onClick} href={item.url} ref={cardRef}>
        <CardImage
          imageUrl={item.img}
        />
        <Card.Content>
          <Card.Text>
            {item.title}
          <Card.Time>{item.relative_time}</Card.Time>
          {
            publisher &&
              <PublisherMeta publisher={publisher} />
          }
          </Card.Text>
        </Card.Content>
      </a>
    </Card.Small>
  )
}

export default function CardSingleArticleMedium (props: Props) {
  const { content }: Props = props

  // no full content no render®
  if (content.length !== 2) {
    return null
  }

  return (
    <Card.ContainerForTwo>
      {
        content.map((item, index) => {
          const shouldScrollIntoView = props.articleToScrollTo && (props.articleToScrollTo.url === item.url)
          const publisher = props.publishers[item.publisher_id]
          return (
            <MediumArticle
              key={index}
              publisher={publisher}
              item={item}
              onReadFeedItem={props.onReadFeedItem}
              shouldScrollIntoView={shouldScrollIntoView}
            />
          )
        })
      }
    </Card.ContainerForTwo>
  )
}
