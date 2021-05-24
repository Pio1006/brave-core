/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

import { getLocale } from 'brave-ui/helpers'

import braveSpinner from './assets/brave-spinner.svg'
import checkIcon from './assets/check.svg'
import smileySadIcon from './assets/smiley-sad.svg'

import {
  StyledBorderlessButton,
  StyledButton,
  StyledCaptchaFrame,
  StyledIcon,
  StyledTitle,
  StyledText,
  StyledValidationSpinner,
  StyledValidationText,
  StyledWrapper
} from './style'

interface Props {
  url: string
  maxAttemptsExceeded: boolean
  onCaptchaAttempt: (result: boolean) => void
  onCaptchaDismissed: () => void
}

interface State {
  showMessage: 'success' | 'maxAttemptsExceeded' | 'validating' | 'none'
}

export default class AdaptiveCaptcha extends React.PureComponent<Props, State> {
  constructor (props: Props) {
    super(props)
    this.state = {
      showMessage: this.props.maxAttemptsExceeded ? 'maxAttemptsExceeded' : 'none',
    }
  }

  componentDidMount () {
    window.addEventListener('message', (event) => {
      // Sandboxed iframes which lack the 'allow-same-origin' header have "null"
      // rather than a valid origin
      if (event.origin !== 'null') {
        return
      }

      const captchaFrame = document.getElementById('scheduled-captcha') as HTMLIFrameElement
      if (!captchaFrame) {
        return
      }

      const captchaContentWindow = captchaFrame.contentWindow
      if (!event.source || event.source !== captchaContentWindow) {
        return
      }

      if (!event.data) {
        return
      }

      switch (event.data) {
        case 'captchaSuccess':
          this.props.onCaptchaAttempt(true)
          this.setState({ showMessage: 'success' })
          break
        case 'captchaFailure':
        case 'error':
          this.props.onCaptchaAttempt(false)
          break
      }
    })
  }

  componentDidUpdate (prevProps: Props, prevState: State) {
    if (!prevProps.maxAttemptsExceeded && this.props.maxAttemptsExceeded) {
      this.setState({ showMessage: 'maxAttemptsExceeded' })
    }
  }

  onClose = () => {
    this.setState({ showMessage: 'none' })
    this.props.onCaptchaDismissed()
  }

  onContactSupport = () => {
    this.setState({ showMessage: 'none' })
    window.open('https://support.brave.com/', '_blank')
  }

  getScheduledCaptcha = () => {
    return (
      <StyledCaptchaFrame
        id='scheduled-captcha'
        src={this.props.url}
        sandbox='allow-scripts'
        scrolling='no'
      />
    )
  }

  getValidatingInterstitial = () => {
    return (
      <StyledWrapper>
        <StyledValidationSpinner src={braveSpinner} />
        <StyledValidationText>
          {getLocale('validating')}
        </StyledValidationText>
      </StyledWrapper>
    )
  }

  getMaxAttemptsExceededMessage = () => {
    return (
      <StyledWrapper>
        <StyledIcon src={smileySadIcon} />
        <StyledTitle>
          {getLocale('captchaMaxAttemptsExceededTitle')}
        </StyledTitle>
        <StyledText>
          {getLocale('captchaMaxAttemptsExceededText')}
        </StyledText>
        <StyledButton onClick={this.onContactSupport}>
          {getLocale('contactSupport')}
        </StyledButton>
      </StyledWrapper>
    )
  }

  getSuccessMessage = () => {
    return (
      <StyledWrapper>
        <StyledIcon src={checkIcon} />
        <StyledTitle textSize='large'>
          {getLocale('captchaSolvedTitle')}
        </StyledTitle>
        <StyledText>
          {getLocale('captchaSolvedText')}
        </StyledText>
        <StyledBorderlessButton onClick={this.onClose}>
          {getLocale('dismiss')}
        </StyledBorderlessButton>
      </StyledWrapper>
    )
  }

  render () {
    switch (this.state.showMessage) {
      case 'success':
        return this.getSuccessMessage()
      case 'maxAttemptsExceeded':
        return this.getMaxAttemptsExceededMessage()
      case 'validating':
        return this.getValidatingInterstitial()
      case 'none':
        break
    }

    return this.getScheduledCaptcha()
  }
}
